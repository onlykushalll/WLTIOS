/**
 * @file tor_proxy.h
 * @brief Tor SOCKS proxy management
 * @module wlt-net/tor_proxy
 * @purpose Configure Tor SOCKS proxy (port 9050), DNS (port 5353),
 *          probe Tor health, send NEWNYM
 * @dependencies: net.h
 * @tested_by: tests/userspace/tor_proxy_test.c
 * @spec: docs/specs/wlt-net.md
 * @adr: docs/adr/03-tor-in-second-vm.md
 */

#ifndef WLT_TOR_PROXY_H
#define WLT_TOR_PROXY_H

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WLT_TOR_CONTROL_PORT 9051
#define WLT_TOR_CONTROL_HOST "127.0.0.1"
#define WLT_TOR_SOCKS_DEFAULT 9050
#define WLT_TOR_DNS_DEFAULT 5353
#define WLT_TOR_BOOTSTRAP_TARGET 100
#define WLT_TOR_CONNECT_TIMEOUT_SEC 30
#define WLT_TOR_NEWNYM_COOLDOWN_SEC 10
#define WLT_TOR_PROBE_MAGIC 0x05
#define WLT_TOR_PROBE_BUF 16

/**
 * Initialize Tor proxy — verifies SOCKS port is reachable.
 * Does NOT block on bootstrap; use wlt_tor_proxy_wait_ready().
 */
wlt_net_error_t wlt_tor_proxy_init(wlt_net_state_t *state);

/**
 * Probe Tor SOCKS port (TCP connect + early SOCKS5 greeting).
 * Updates state->tor_state. No DNS, no remote traffic — just local TCP.
 */
wlt_net_error_t wlt_tor_proxy_check_status(wlt_net_state_t *state);

/**
 * Send NEWNYM signal to Tor control port for new circuits.
 * Cooldown-enforced to respect Tor's rate limit.
 */
wlt_net_error_t wlt_tor_proxy_new_identity(wlt_net_state_t *state);

/**
 * Wait until state->tor_state == WLT_NET_TOR_READY or timeout.
 */
wlt_net_error_t wlt_tor_proxy_wait_ready(wlt_net_state_t *state,
                                          uint32_t timeout_sec);

/**
 * Render the SOCKS proxy URL (e.g. "socks5://127.0.0.1:9050").
 */
wlt_net_error_t wlt_tor_proxy_get_url(const wlt_net_state_t *state,
                                       char *buf,
                                       size_t buf_size);

/**
 * Release Tor proxy resources (currently none — kept for symmetry).
 */
wlt_net_error_t wlt_tor_proxy_cleanup(wlt_net_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* WLT_TOR_PROXY_H */
