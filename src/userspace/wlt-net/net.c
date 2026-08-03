/**
 * @file net.c
 * @brief Main network manager — logging, CLI dispatch, event loop
 * @module wlt-net
 * @purpose Entry point for `wltnet` daemon; polls Tor health, triggers
 *          kill switch within 100ms of Tor disconnect
 * @dependencies: net.h, wifi.h, tor_proxy.h, mac.h
 * @tested_by: tests/userspace/wlt-net_test.c
 * @spec: docs/specs/wlt-net.md
 * @adr: docs/adr/03-tor-in-second-vm.md, docs/adr/07-custom-init.md
 */

#include "net.h"
#include "wifi.h"
#include "tor_proxy.h"
#include "mac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ─── Logging ────────────────────────────────────────── */

static FILE *g_log_file = NULL;
static volatile sig_atomic_t g_shutdown = 0;

void
wlt_net_log_init(const char *path)
{
    if (path == NULL)
    {
        g_log_file = stderr;
        return;
    }
    mkdir("/var/log/wlt", 0755);
    g_log_file = fopen(path, "a");
    if (g_log_file == NULL)
        g_log_file = stderr;
}

void
wlt_net_log_close(void)
{
    if (g_log_file != NULL && g_log_file != stderr)
        fclose(g_log_file);
    g_log_file = NULL;
}

void
wlt_net_log(wlt_net_log_level_t level, const char *fmt, ...)
{
    if (g_log_file == NULL)
        g_log_file = stderr;
    static const char *level_str[] = {
        "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };
    if ((int)level < 0 || (int)level > 4)
        level = WLT_NET_LOG_ERROR;
    time_t now;
    struct tm tm;
    char timebuf[32];
    time(&now);
    localtime_r(&now, &tm);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(g_log_file, "[%s] [%s] ", timebuf, level_str[level]);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);
    fprintf(g_log_file, "\n");
    fflush(g_log_file);
}

static void
on_signal(int sig)
{
    (void)sig;
    g_shutdown = 1;
}

/* ─── Helpers ────────────────────────────────────────── */

const char *
wlt_net_error_str(wlt_net_error_t err)
{
    switch (err)
    {
        case WLT_NET_OK:                  return "ok";
        case WLT_NET_ERR_INVALID_ARG:     return "invalid argument";
        case WLT_NET_ERR_NOT_FOUND:       return "not found";
        case WLT_NET_ERR_IO:              return "I/O error";
        case WLT_NET_ERR_NOMEM:           return "out of memory";
        case WLT_NET_ERR_PERMISSION:      return "permission denied";
        case WLT_NET_ERR_TIMEOUT:         return "timeout";
        case WLT_NET_ERR_TOR_NOT_READY:   return "Tor not ready";
        case WLT_NET_ERR_WIFI_HW_MISSING: return "WiFi hardware missing";
        case WLT_NET_ERR_WIFI_CONNECT:    return "WiFi connect failed";
        case WLT_NET_ERR_PARSE:           return "parse error";
        case WLT_NET_ERR_KILL_SWITCH:     return "kill switch error";
        case WLT_NET_ERR_UNSUPPORTED:     return "unsupported";
        default:                          return "unknown";
    }
}

const char *
wlt_net_link_str(wlt_net_link_type_t link)
{
    switch (link)
    {
        case WLT_NET_LINK_DOWN:     return "down";
        case WLT_NET_LINK_WIFI:     return "wifi";
        case WLT_NET_LINK_ETHERNET: return "ethernet";
        default:                    return "unknown";
    }
}

const char *
wlt_net_tor_state_str(wlt_net_tor_state_t tor)
{
    switch (tor)
    {
        case WLT_NET_TOR_DISCONNECTED: return "disconnected";
        case WLT_NET_TOR_CONNECTING:   return "connecting";
        case WLT_NET_TOR_READY:        return "ready";
        case WLT_NET_TOR_FAILED:       return "failed";
        default:                       return "unknown";
    }
}

wlt_net_error_t
wlt_net_run_cmd(const char *path, char *const argv[], int *exit_code)
{
    if (path == NULL || argv == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    pid_t pid = fork();
    if (pid < 0)
    {
        WLT_NET_LOG_ERR("fork() failed: %s", strerror(errno));
        return WLT_NET_ERR_IO;
    }
    if (pid == 0)
    {
        /* Child: silence stdout/stderr, then exec. */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execv(path, argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno == EINTR)
            continue;
        WLT_NET_LOG_ERR("waitpid() failed: %s", strerror(errno));
        return WLT_NET_ERR_IO;
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exit_code != NULL)
        *exit_code = code;
    if (code != 0)
        WLT_NET_LOG_DBG("cmd %s exit %d", path, code);
    return WLT_NET_OK;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_net_error_t
wlt_net_init(wlt_net_state_t *state, const char *config_path)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    wlt_net_error_t err = wlt_net_config_defaults(state);
    if (err != WLT_NET_OK)
        return err;
    err = wlt_net_config_load(state, config_path);
    if (err != WLT_NET_OK)
        return err;

    /* nftables preferred, iptables fallback. */
    state->use_nftables = wlt_kill_switch_nftables_available();
    WLT_NET_LOG_INF("Firewall backend: %s",
                     state->use_nftables ? "nftables" : "iptables");

    /* Install fail-closed rules — Tor-only from the very first moment. */
    if (state->kill_switch_enabled)
    {
        err = wlt_kill_switch_install(state);
        if (err != WLT_NET_OK)
        {
            WLT_NET_LOG_FTL("kill switch install failed: %s",
                             wlt_net_error_str(err));
            return err;
        }
    }

    /* MAC randomization on boot — privacy from first packet. */
    if ((state->mac_mode == WLT_NET_MAC_ON_BOOT ||
         state->mac_mode == WLT_NET_MAC_ALWAYS) &&
        wlt_wifi_hardware_present())
    {
        (void)wlt_mac_randomize(state->iface);
        uint8_t mac[WLT_MAC_LEN];
        if (wlt_mac_get(state->iface, mac) == WLT_NET_OK)
            wlt_mac_format(mac, state->current_mac,
                           sizeof(state->current_mac));
    }

    /* WiFi — optional, may be unavailable on headless boxes. */
    if (wlt_wifi_hardware_present())
    {
        err = wlt_wifi_init(state);
        if (err != WLT_NET_OK)
            WLT_NET_LOG_WRN("WiFi init failed: %s (WiFi disabled)",
                             wlt_net_error_str(err));
    }
    else
        WLT_NET_LOG_INF("No WiFi hardware detected — WiFi disabled");

    (void)wlt_ethernet_init(state);
    (void)wlt_tor_proxy_init(state);

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    state->running = true;
    WLT_NET_LOG_INF("WLT-Net v%s initialized", WLT_NET_VERSION);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_net_run_loop(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    WLT_NET_LOG_INF("Entering event loop");

    while (state->running && !g_shutdown)
    {
        wlt_net_tor_state_t prev = state->tor_state;
        wlt_net_error_t err = wlt_tor_proxy_check_status(state);
        if (err != WLT_NET_OK)
            WLT_NET_LOG_DBG("tor probe err: %s", wlt_net_error_str(err));

        /* Tor died — trigger kill switch within 100ms budget. */
        if (prev == WLT_NET_TOR_READY &&
            state->tor_state != WLT_NET_TOR_READY &&
            state->kill_switch_enabled)
        {
            WLT_NET_LOG_FTL("Tor down — activating kill switch");
            (void)wlt_net_kill_switch_trigger(state);
        }
        /* Tor recovered — release kill switch. */
        else if (state->kill_switch_active &&
                 state->tor_state == WLT_NET_TOR_READY)
        {
            WLT_NET_LOG_INF("Tor recovered — releasing kill switch");
            (void)wlt_net_kill_switch_release(state);
        }
        usleep(WLT_NET_TOR_CHECK_INTERVAL_MS * 1000);
    }
    WLT_NET_LOG_INF("Event loop exited");
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_net_shutdown(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    WLT_NET_LOG_INF("Shutting down WLT-Net");
    state->running = false;
    (void)wlt_wifi_disconnect(state);
    (void)wlt_wifi_cleanup(state);
    (void)wlt_tor_proxy_cleanup(state);
    /* Re-apply fail-closed rules so we never leave the box open.
     * [fix] Previously this block called wlt_kill_switch_cleanup()
     * immediately after apply_block(), which either deletes the
     * nftables table outright or sets iptables policies back to
     * ACCEPT - undoing the block on the very next line, directly
     * against this comment's own intent. On a Tor-by-default OS,
     * every shutdown/restart was leaving a window with zero firewall
     * enforcement right before the daemon exits. Do not clean up the
     * block on shutdown - leave the box blocked until the next
     * explicit start re-establishes real connectivity. */
    if (state->kill_switch_enabled)
        (void)wlt_kill_switch_apply_block(state);
    return WLT_NET_OK;
}

wlt_net_error_t
wlt_net_get_status(const wlt_net_state_t *state,
                   char *buf, size_t buf_size)
{
    if (state == NULL || buf == NULL || buf_size == 0)
        return WLT_NET_ERR_INVALID_ARG;
    const char *ssid = (state->link == WLT_NET_LINK_WIFI &&
                         state->connected_ssid[0] != '\0')
                            ? state->connected_ssid : "-";
    int n = snprintf(buf, buf_size,
                     "link=%s tor=%s kill=%s ssid=%s ip=%s mac=%s",
                     wlt_net_link_str(state->link),
                     wlt_net_tor_state_str(state->tor_state),
                     state->kill_switch_active ? "ACTIVE" : "idle",
                     ssid,
                     state->ip_addr[0] ? state->ip_addr : "-",
                     state->current_mac[0] ? state->current_mac : "-");
    return (n < 0) ? WLT_NET_ERR_IO : WLT_NET_OK;
}

wlt_net_error_t
wlt_net_tor_check_status(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    return wlt_tor_proxy_check_status(state);
}

wlt_net_error_t
wlt_net_kill_switch_trigger(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    return wlt_kill_switch_apply_block(state);
}

wlt_net_error_t
wlt_net_kill_switch_release(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    /* Fail-closed: never release unless Tor is actually ready. */
    if (state->tor_state != WLT_NET_TOR_READY)
    {
        WLT_NET_LOG_WRN("Refusing to release kill switch — Tor not ready");
        return WLT_NET_ERR_TOR_NOT_READY;
    }
    return wlt_kill_switch_apply_tor_only(state);
}

wlt_net_error_t
wlt_net_tor_new_identity(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    return wlt_tor_proxy_new_identity(state);
}

wlt_net_error_t
wlt_net_mac_randomize_now(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;
    return wlt_mac_randomize(state->iface);
}

#ifndef WLT_TESTING
/* ─── CLI ────────────────────────────────────────────── */

static int
cmd_status(wlt_net_state_t *state)
{
    char buf[256];
    if (wlt_net_get_status(state, buf, sizeof(buf)) != WLT_NET_OK)
        return 1;
    printf("%s\n", buf);
    return 0;
}

static int
cmd_wifi_scan(wlt_net_state_t *state)
{
    wlt_net_error_t err = wlt_wifi_scan(state);
    if (err != WLT_NET_OK)
    {
        fprintf(stderr, "scan failed: %s\n", wlt_net_error_str(err));
        return 1;
    }
    for (size_t i = 0; i < state->scan_count; i++)
    {
        const wlt_net_wifi_network_t *n = &state->scan_results[i];
        char bssid[WLT_MAC_STR_LEN];
        wlt_mac_format(n->bssid, bssid, sizeof(bssid));
        printf("%-32s %4ddBm %s ch=%u %s\n",
               n->ssid, n->signal_dbm, bssid, n->channel,
               n->secure ? "[WPA]" : "[OPEN]");
    }
    return 0;
}

static int
cmd_wifi_connect(wlt_net_state_t *s, const char *ssid, const char *psk)
{
    wlt_net_error_t err = wlt_wifi_connect(s, ssid, psk);
    if (err != WLT_NET_OK)
    {
        fprintf(stderr, "connect failed: %s\n", wlt_net_error_str(err));
        return 1;
    }
    printf("connected to %s\n", ssid);
    return 0;
}

static void
usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <command> [args]\n"
        "Commands:\n"
        "  status                     Show network status\n"
        "  daemon                     Run as background daemon\n"
        "  wifi scan                  Scan for WiFi networks\n"
        "  wifi connect <ssid> [psk]  Connect to WiFi\n"
        "  wifi disconnect            Disconnect WiFi\n"
        "  wifi forget <ssid>         Forget saved WiFi\n"
        "  tor status                 Show Tor status\n"
        "  tor new-identity           Request new Tor circuits\n"
        "  kill-switch                Manually trigger kill switch\n"
        "  mac randomize              Randomize MAC address now\n",
        prog);
}

int
main(int argc, char **argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    wlt_net_log_init(WLT_NET_LOG_PATH);

    wlt_net_state_t state;
    wlt_net_error_t err = wlt_net_init(&state, NULL);
    if (err != WLT_NET_OK)
    {
        fprintf(stderr, "init failed: %s\n", wlt_net_error_str(err));
        return 1;
    }

    int rc = 0;
    const char *cmd = argv[1];

    if (strcmp(cmd, "status") == 0)
        rc = cmd_status(&state);
    else if (strcmp(cmd, "daemon") == 0)
        rc = (wlt_net_run_loop(&state) == WLT_NET_OK) ? 0 : 1;
    else if (strcmp(cmd, "wifi") == 0 && argc >= 3)
    {
        if (strcmp(argv[2], "scan") == 0)
            rc = cmd_wifi_scan(&state);
        else if (strcmp(argv[2], "connect") == 0 && argc >= 4)
            rc = cmd_wifi_connect(&state, argv[3],
                                   (argc >= 5) ? argv[4] : NULL);
        else if (strcmp(argv[2], "disconnect") == 0)
            rc = (wlt_wifi_disconnect(&state) == WLT_NET_OK) ? 0 : 1;
        else if (strcmp(argv[2], "forget") == 0 && argc >= 4)
            rc = (wlt_wifi_forget(&state, argv[3]) == WLT_NET_OK) ? 0 : 1;
        else
            usage(argv[0]), rc = 1;
    }
    else if (strcmp(cmd, "tor") == 0 && argc >= 3)
    {
        if (strcmp(argv[2], "status") == 0)
        {
            (void)wlt_net_tor_check_status(&state);
            printf("tor=%s\n", wlt_net_tor_state_str(state.tor_state));
        }
        else if (strcmp(argv[2], "new-identity") == 0)
            rc = (wlt_net_tor_new_identity(&state) == WLT_NET_OK) ? 0 : 1;
        else
            usage(argv[0]), rc = 1;
    }
    else if (strcmp(cmd, "kill-switch") == 0)
        rc = (wlt_net_kill_switch_trigger(&state) == WLT_NET_OK) ? 0 : 1;
    else if (strcmp(cmd, "mac") == 0 && argc >= 3 &&
             strcmp(argv[2], "randomize") == 0)
        rc = (wlt_net_mac_randomize_now(&state) == WLT_NET_OK) ? 0 : 1;
    else
        usage(argv[0]), rc = 1;

    (void)wlt_net_shutdown(&state);
    wlt_net_log_close();
    return rc;
}
#endif /* WLT_TESTING */
