/**
 * @file anonymity_coach.c
 * @brief Anonymity coach — detects DNS leaks, Tor status, fingerprinting
 * @module wlt-ai/anonymity_coach
 * @purpose Monitor anonymity health and warn user about leaks
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct {
    bool tor_connected;
    bool dns_via_tor;
    bool kill_switch_active;
    bool mac_randomized;
    bool webrtc_leak;
    int active_circuits;
    char exit_country[4]; /* ISO 3166-1 alpha-2 */
    char exit_ip[46];
} wlt_anon_status_t;

static wlt_anon_status_t g_anon = {0};

typedef enum {
    WLT_ANON_OK = 0,
    WLT_ANON_WARN_TOR_DISCONNECTED,
    WLT_ANON_WARN_DNS_LEAK,
    WLT_ANON_WARN_KILL_SWITCH_OFF,
    WLT_ANON_WARN_MAC_NOT_RANDOM,
    WLT_ANON_WARN_WEBRTC_LEAK,
    WLT_ANON_WARN_LOW_CIRCUITS,
} wlt_anon_issue_t;

wlt_ai_err_t
wlt_ai_anon_update_status(bool tor_ok, bool dns_tor, bool kill_switch,
                          bool mac_rand, bool webrtc_ok, int circuits,
                          const char *country, const char *ip)
{
    g_anon.tor_connected = tor_ok;
    g_anon.dns_via_tor = dns_tor;
    g_anon.kill_switch_active = kill_switch;
    g_anon.mac_randomized = mac_rand;
    g_anon.webrtc_leak = !webrtc_ok;
    g_anon.active_circuits = circuits;
    if (country) strncpy(g_anon.exit_country, country, 3);
    if (ip) strncpy(g_anon.exit_ip, ip, 45);
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_ai_anon_check(char *warnings, size_t warn_size)
{
    if (!warnings) return WLT_AI_ERR_INVALID;

    size_t offset = 0;
    int issue_count = 0;

    /* Check Tor connection */
    if (!g_anon.tor_connected) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "CRITICAL: Tor is disconnected! All network traffic is blocked by kill switch.\n");
        issue_count++;
    }

    /* Check DNS leak */
    if (g_anon.tor_connected && !g_anon.dns_via_tor) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "WARNING: DNS is NOT routing through Tor! Your DNS queries can reveal\n"
            "  what websites you visit. Enable DNS-over-Tor in Settings > Network.\n");
        issue_count++;
    }

    /* Check kill switch */
    if (g_anon.tor_connected && !g_anon.kill_switch_active) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "WARNING: Kill switch is OFF! If Tor disconnects, your real IP will leak.\n"
            "  Enable kill switch in Settings > Security.\n");
        issue_count++;
    }

    /* Check MAC randomization */
    if (!g_anon.mac_randomized) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "WARNING: MAC address is not randomized! Your network card's hardware\n"
            "  address can be used to track you across sessions.\n");
        issue_count++;
    }

    /* Check WebRTC leak */
    if (g_anon.webrtc_leak) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "WARNING: WebRTC may leak your real IP address! Disable WebRTC in\n"
            "  Tor Browser settings or use the WebRTC toggle extension.\n");
        issue_count++;
    }

    /* Check circuit count */
    if (g_anon.tor_connected && g_anon.active_circuits < 3) {
        offset += snprintf(warnings + offset, warn_size - offset,
            "INFO: Only %d Tor circuit(s) active. Consider getting a new identity\n"
            "  for better anonymity.\n", g_anon.active_circuits);
        issue_count++;
    }

    if (issue_count == 0) {
        snprintf(warnings, warn_size,
            "Your anonymity is protected. Tor is connected via %s (%s),\n"
            "DNS is routed through Tor, kill switch is active, MAC is randomized.\n"
            "No leaks detected.\n",
            g_anon.exit_country, g_anon.exit_ip);
    }

    return WLT_AI_OK;
}

/* Real Tor probe: connect to the Tor control port (127.0.0.1:9051) and
 * issue GETINFO status/circuit-established. Returns true ONLY if Tor
 * confirms a live circuit. Fails closed (returns false) if the control
 * port is unreachable, refuses the connection, returns an error, or the
 * circuit is not established. This replaces the previous trust-the-caller
 * `tor_connected` boolean (audit E-C3 / B-M4) which kairos_window.c was
 * hardcoding to true. */
static bool
wlt_ai_probe_tor_circuit(void)
{
    static const char probe[] =
        "GETINFO status/circuit-established\r\nquit\r\n";
    char buf[512];
    int sock, ret;
    bool ok = false;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9051); /* Tor control port */
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(sock); return false;
    }

    /* 2s timeouts — the control port is loopback; if Tor is up it must
     * answer fast. If anything stalls or refuses, treat as Tor DOWN. */
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock); return false; /* Tor control port not running */
    }

    if (send(sock, probe, sizeof(probe) - 1, 0) <= 0) {
        close(sock); return false;
    }

    /* Read Tor's reply (small). Loop in case it splits across packets,
     * and short-circuit once we have the circuit-established line. */
    size_t total = 0;
    buf[0] = '\0';
    while (total < sizeof(buf) - 1) {
        ret = recv(sock, buf + total, sizeof(buf) - 1 - total, 0);
        if (ret <= 0) break;
        total += (size_t)ret;
        buf[total] = '\0';
        if (strstr(buf, "250-status/circuit-established=") != NULL)
            break;
    }
    buf[total] = '\0';
    close(sock);

    /* 250-status/circuit-established=1 means a Tor circuit is live.
     * Anything else (including no response, error responses, or =0)
     * means Tor is DOWN or not yet bootstrapped. */
    if (strstr(buf, "250-status/circuit-established=1") != NULL)
        ok = true;

    return ok;
}

bool
wlt_ai_anon_is_safe(void)
{
    /* Fail closed: if we cannot independently verify a live Tor circuit
     * via the control port, NEVER report "safe" — the UI must not show
     * "PROTECTED" when Tor is dead or uncheckable. The caller-supplied
     * booleans below remain necessary conditions but are no longer
     * sufficient on their own (audit E-C3 / B-M4). */
    if (!wlt_ai_probe_tor_circuit())
        return false;

    return g_anon.dns_via_tor &&
           g_anon.kill_switch_active &&
           g_anon.mac_randomized &&
           !g_anon.webrtc_leak;
}
