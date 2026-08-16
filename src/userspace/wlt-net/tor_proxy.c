/**
 * @file tor_proxy.c
 * @brief Tor SOCKS proxy management
 * @module wlt-net/tor_proxy
 * @purpose Probe Tor SOCKS port (9050) for health, send NEWNYM to the
 *          control port (9051) for fresh circuits.
 * @dependencies: net.h
 * @tested_by: tests/userspace/tor_proxy_test.c
 * @spec: docs/specs/wlt-net.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 *
 * All probing is local — TCP connect to 127.0.0.1:<port>. No DNS, no
 * remote traffic, no leaks. We send a SOCKS5 greeting to distinguish
 * "Tor listening" from "port forwarded to something else".
 */

#include "net.h"
#include "tor_proxy.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>

/* ─── Helpers ────────────────────────────────────────── */

static wlt_net_error_t
tcp_connect_localhost(uint16_t port, int *out_fd)
{
    if (out_fd == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    *out_fd = -1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return WLT_NET_ERR_IO;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, WLT_NET_TOR_SOCKS_HOST,
                  &addr.sin_addr) != 1)
    {
        close(fd);
        return WLT_NET_ERR_IO;
    }

    /* Short, non-blocking connect for fast health probing. */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS)
    {
        close(fd);
        return WLT_NET_ERR_TOR_NOT_READY;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
    if (poll(&pfd, 1, 200) <= 0)
    {
        close(fd);
        return WLT_NET_ERR_TOR_NOT_READY;
    }
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0)
    {
        close(fd);
        return WLT_NET_ERR_TOR_NOT_READY;
    }

    /* Restore blocking. */
    if (flags >= 0)
        (void)fcntl(fd, F_SETFL, flags);

    *out_fd = fd;
    return WLT_NET_OK;
}

static wlt_net_error_t
tor_socks_greeting(int fd)
{
    /* SOCKS5 client greeting: VER=5, NMETHODS=1, METHOD=0 (NO-AUTH) */
    uint8_t hello[3] = { 0x05, 0x01, 0x00 };
    if (write(fd, hello, sizeof(hello)) != (ssize_t)sizeof(hello))
        return WLT_NET_ERR_IO;

    uint8_t resp[WLT_TOR_PROBE_BUF];
    struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    if (poll(&pfd, 1, 200) <= 0)
        return WLT_NET_ERR_TOR_NOT_READY;

    ssize_t n = read(fd, resp, sizeof(resp));
    if (n < 2)
        return WLT_NET_ERR_TOR_NOT_READY;
    /* Server must echo VER=5 and select METHOD=0 (no-auth).
     * METHOD=0xFF means Tor rejected our greeting (no acceptable
     * methods); METHOD=0x02 means it requires username/password auth.
     * Either way Tor is NOT usable as an open SOCKS proxy - treating
     * that as "ready" would be a false positive that could release the
     * kill switch while Tor cant actually proxy, defeating Tor-by-default. */
    if (resp[0] != 0x05 || resp[1] != 0x00)
        return WLT_NET_ERR_TOR_NOT_READY;
    return WLT_NET_OK;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_net_error_t
wlt_tor_proxy_init(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    state->tor_state = WLT_NET_TOR_CONNECTING;
    wlt_net_error_t err = wlt_tor_proxy_check_status(state);
    if (err != WLT_NET_OK)
        WLT_NET_LOG_DBG("tor probe on init: %s", wlt_net_error_str(err));
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_tor_proxy_check_status(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    int fd = -1;
    wlt_net_error_t err = tcp_connect_localhost(state->tor_socks_port, &fd);
    if (err != WLT_NET_OK)
    {
        state->tor_state = WLT_NET_TOR_DISCONNECTED;
        return err;
    }

    err = tor_socks_greeting(fd);
    close(fd);
    if (err != WLT_NET_OK)
    {
        state->tor_state = WLT_NET_TOR_DISCONNECTED;
        return err;
    }

    state->tor_state = WLT_NET_TOR_READY;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_tor_proxy_new_identity(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    /* Cooldown — Tor limits NEWNYM to one per 10s. */
    static time_t last_newnym = 0;
    time_t now = time(NULL);
    if (last_newnym != 0 && (now - last_newnym) < WLT_TOR_NEWNYM_COOLDOWN_SEC)
    {
        WLT_NET_LOG_WRN("new-identity on cooldown (%lds left)",
                         (long)(WLT_TOR_NEWNYM_COOLDOWN_SEC -
                                (now - last_newnym)));
        return WLT_NET_ERR_TIMEOUT;
    }
    last_newnym = now;

    int fd = -1;
    wlt_net_error_t err = tcp_connect_localhost(WLT_TOR_CONTROL_PORT, &fd);
    if (err != WLT_NET_OK)
    {
        WLT_NET_LOG_ERR("cannot reach Tor control port");
        return err;
    }

    /* Authenticate (no cookie — relies on control port being unauthenticated
     * on the loopback-only Tor VM. A production deployment should use
     * CookieAuthentication — TODO(wlt-net): read cookie from
     * /var/lib/tor/control_auth_cookie). */
    const char *auth = "AUTHENTICATE \"\"\r\n";
    if (write(fd, auth, strlen(auth)) != (ssize_t)strlen(auth))
    {
        close(fd);
        return WLT_NET_ERR_IO;
    }

    char buf[WLT_TOR_PROBE_BUF];
    memset(buf, 0, sizeof(buf));
    /* [fix] Use poll() with a timeout - a blocking read() here would
     * hang the caller indefinitely if the Tor control port stalls
     * (e.g. Tor died after accept()). The SOCKS probe path already
     * uses poll(200ms); the control port path was missing it. */
    struct pollfd auth_pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    if (poll(&auth_pfd, 1, 2000) <= 0)
    {
        close(fd);
        WLT_NET_LOG_ERR("Tor control port AUTHENTICATE timeout");
        return WLT_NET_ERR_TIMEOUT;
    }
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0 || strncmp(buf, "250", 3) != 0)
    {
        close(fd);
        WLT_NET_LOG_ERR("Tor AUTHENTICATE rejected");
        return WLT_NET_ERR_PERMISSION;
    }

    const char *newnym = "SIGNAL NEWNYM\r\n";
    if (write(fd, newnym, strlen(newnym)) != (ssize_t)strlen(newnym))
    {
        close(fd);
        return WLT_NET_ERR_IO;
    }
    struct pollfd ny_pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
    if (poll(&ny_pfd, 1, 2000) <= 0)
    {
        close(fd);
        WLT_NET_LOG_ERR("Tor control port NEWNYM timeout");
        return WLT_NET_ERR_TIMEOUT;
    }
    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0 || strncmp(buf, "250", 3) != 0)
    {
        WLT_NET_LOG_ERR("Tor NEWNYM rejected");
        return WLT_NET_ERR_IO;
    }

    WLT_NET_LOG_INF("Tor NEWNYM sent — new circuits requested");
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_tor_proxy_wait_ready(wlt_net_state_t *state, uint32_t timeout_sec)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    uint32_t elapsed = 0;
    while (elapsed < timeout_sec * 1000)
    {
        wlt_net_error_t err = wlt_tor_proxy_check_status(state);
        if (err == WLT_NET_OK && state->tor_state == WLT_NET_TOR_READY)
            return WLT_NET_OK;
        usleep(500000);  /* 500ms */
        elapsed += 500;
    }
    return WLT_NET_ERR_TIMEOUT;
}

wlt_net_error_t
wlt_tor_proxy_get_url(const wlt_net_state_t *state,
                      char *buf, size_t buf_size)
{
    if (state == NULL || buf == NULL || buf_size == 0)
        return WLT_NET_ERR_INVALID_ARG;

    int n = snprintf(buf, buf_size, "socks5://%s:%u",
                     WLT_NET_TOR_SOCKS_HOST, state->tor_socks_port);
    if (n < 0 || (size_t)n >= buf_size)
        return WLT_NET_ERR_IO;
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_tor_proxy_cleanup(wlt_net_state_t *state)
{
    /* No persistent resources — socket is opened per-probe. */
    (void)state;
    return WLT_NET_OK;
}
