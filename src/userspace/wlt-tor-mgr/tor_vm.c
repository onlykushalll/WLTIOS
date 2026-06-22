/**
 * @file tor_vm.c
 * @brief Tor VM configuration — move Tor from main Linux to isolated VM
 * @module wlt-tor-mgr/tor_vm
 * @purpose Configure and manage the second Linux VM running Tor daemon
 * @dependencies: seL4 VMM (libvmm or CAmkES VMM)
 * @spec: docs/specs/wlt-tor-mgr.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 *
 * Architecture (per ADR-03):
 * - Main Linux guest (GUI, apps, user workspace)
 * - Tor VM (second Linux guest, minimal, runs Tor daemon only)
 * - Both isolated by seL4
 * - Network: Main Linux → Tor VM → NIC (via Firewall PD)
 * - Tor identity keys stored in Key Vault PD (seL4)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

typedef enum {
    WLT_TORVM_OK = 0,
    WLT_TORVM_ERR_INVALID = -1,
    WLT_TORVM_ERR_IO = -2,
    WLT_TORVM_ERR_TIMEOUT = -3,
    WLT_TORVM_ERR_NOT_READY = -4,
} wlt_torvm_error_t;

#define TORVM_SOCKS_PORT 9050
#define TORVM_DNS_PORT 5353
#define TORVM_CONTROL_PORT 9051
#define TORVM_CHECK_INTERVAL_MS 500
#define TORVM_STARTUP_TIMEOUT_MS 30000

typedef enum {
    WLT_TORVM_STATE_STOPPED = 0,
    WLT_TORVM_STATE_STARTING,
    WLT_TORVM_STATE_READY,
    WLT_TORVM_STATE_FAILED,
} wlt_torvm_state_t;

static wlt_torvm_state_t g_torvm_state = WLT_TORVM_STATE_STOPPED;

/* Check if Tor VM SOCKS port is accepting connections */
static bool
torvm_port_open(uint16_t port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(0x7F000001);  /* 127.0.0.1 */

    /* Set non-blocking */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };  /* 100ms */

    bool result = false;
    if (select(sock + 1, NULL, &fds, NULL, &tv) > 0)
    {
        int err;
        socklen_t err_len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &err_len);
        result = (err == 0);
    }

    close(sock);
    return result;
}

/* Check if Tor VM is ready (SOCKS port open + DNS port open) */
wlt_torvm_state_t
wlt_torvm_get_state(void)
{
    if (g_torvm_state != WLT_TORVM_STATE_STARTING &&
        g_torvm_state != WLT_TORVM_STATE_READY)
        return g_torvm_state;

    if (torvm_port_open(TORVM_SOCKS_PORT) && torvm_port_open(TORVM_DNS_PORT))
    {
        g_torvm_state = WLT_TORVM_STATE_READY;
    }

    return g_torvm_state;
}

/* Wait for Tor VM to become ready */
wlt_torvm_error_t
wlt_torvm_wait_ready(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms)
    {
        if (wlt_torvm_get_state() == WLT_TORVM_STATE_READY)
            return WLT_TORVM_OK;
        usleep(TORVM_CHECK_INTERVAL_MS * 1000);
        elapsed += TORVM_CHECK_INTERVAL_MS;
    }
    return WLT_TORVM_ERR_TIMEOUT;
}

/* Start the Tor VM (via seL4 VMM) */
wlt_torvm_error_t
wlt_torvm_start(void)
{
    /* TODO(wlt-tor-mgr): Start Tor VM via seL4 VMM:
     *   1. Request Key Vault PD to provide Tor identity key
     *   2. Create new Linux VM via CAmkES/libvmm
     *   3. Load minimal Linux + Tor daemon into VM
     *   4. Configure Tor with identity key from Key Vault
     *   5. Wait for SOCKS port to open
     */

    g_torvm_state = WLT_TORVM_STATE_STARTING;

    /* For now, check if Tor is already running (dev mode) */
    if (torvm_port_open(TORVM_SOCKS_PORT))
    {
        g_torvm_state = WLT_TORVM_STATE_READY;
        return WLT_TORVM_OK;
    }

    return WLT_TORVM_ERR_NOT_READY;
}

/* Stop the Tor VM */
wlt_torvm_error_t
wlt_torvm_stop(void)
{
    /* TODO(wlt-tor-mgr): Stop Tor VM via seL4 VMM */
    g_torvm_state = WLT_TORVM_STATE_STOPPED;
    return WLT_TORVM_OK;
}

/* Restart the Tor VM (get new identity) */
wlt_torvm_error_t
wlt_torvm_restart(void)
{
    wlt_torvm_stop();
    usleep(1000000);  /* 1 second */
    return wlt_torvm_start();
}

/* Get Tor VM SOCKS proxy address */
const char *
wlt_torvm_get_socks_addr(void)
{
    return "127.0.0.1:" "9050";
}

/* Get Tor VM DNS address */
const char *
wlt_torvm_get_dns_addr(void)
{
    return "127.0.0.1:" "5353";
}
