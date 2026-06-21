/**
 * @file net.h
 * @brief Public API for WLT-Net (network manager)
 * @module wlt-net
 * @purpose Manage WiFi/Ethernet links, route ALL traffic through Tor,
 *          trigger kill switch when Tor dies
 * @dependencies: wifi.h, tor_proxy.h, mac.h
 * @tested_by: tests/userspace/wlt-net_test.c
 * @spec: docs/specs/wlt-net.md
 * @adr: docs/adr/03-tor-in-second-vm.md, docs/adr/07-custom-init.md
 *
 * NOTE: Spec originally proposed a Rust implementation. Per project
 * direction, this module is written in C with the same responsibilities.
 */

#ifndef WLT_NET_H
#define WLT_NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_NET_OK = 0,
    WLT_NET_ERR_INVALID_ARG = -1,
    WLT_NET_ERR_NOT_FOUND = -2,
    WLT_NET_ERR_IO = -3,
    WLT_NET_ERR_NOMEM = -4,
    WLT_NET_ERR_PERMISSION = -5,
    WLT_NET_ERR_TIMEOUT = -6,
    WLT_NET_ERR_TOR_NOT_READY = -7,
    WLT_NET_ERR_WIFI_HW_MISSING = -8,
    WLT_NET_ERR_WIFI_CONNECT = -9,
    WLT_NET_ERR_PARSE = -10,
    WLT_NET_ERR_KILL_SWITCH = -11,
    WLT_NET_ERR_UNSUPPORTED = -12,
} wlt_net_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_NET_VERSION "1.0.0"
#define WLT_NET_CONFIG_PATH "/etc/wlt/network.conf"
#define WLT_NET_LOG_PATH "/var/log/wlt/wlt-net.log"
#define WLT_NET_MAX_IFACE 16
#define WLT_NET_MAX_SSID 64
#define WLT_NET_MAX_IP 64
#define WLT_NET_MAX_MAC_STR 18
#define WLT_NET_MAX_BSSID 6
#define WLT_NET_MAX_SCAN 64
#define WLT_NET_MAX_LINE 512
#define WLT_NET_DEFAULT_TOR_SOCKS_PORT 9050
#define WLT_NET_DEFAULT_TOR_DNS_PORT 5353
#define WLT_NET_SCAN_RETRIES 3
#define WLT_NET_CONNECT_RETRIES 3
#define WLT_NET_SCAN_TIMEOUT_SEC 3
#define WLT_NET_CONNECT_TIMEOUT_SEC 5
#define WLT_NET_TOR_CHECK_INTERVAL_MS 500
#define WLT_NET_KILL_SWITCH_LATENCY_MS 100
#define WLT_NET_TOR_SOCKS_HOST "127.0.0.1"
#define WLT_NET_IP_CMD "/usr/sbin/ip"
#define WLT_NET_IPTABLES_CMD "/usr/sbin/iptables"
#define WLT_NET_NFT_CMD "/usr/sbin/nft"
#define WLT_NET_IW_CMD "/usr/sbin/iw"
#define WLT_NET_UDHCPC_CMD "/usr/sbin/udhcpc"
#define WLT_NET_DHCLIENT_CMD "/usr/sbin/dhclient"

/* ─── Types ──────────────────────────────────────────── */

typedef enum {
    WLT_NET_LINK_DOWN = 0,
    WLT_NET_LINK_WIFI,
    WLT_NET_LINK_ETHERNET,
} wlt_net_link_type_t;

typedef enum {
    WLT_NET_TOR_DISCONNECTED = 0,
    WLT_NET_TOR_CONNECTING,
    WLT_NET_TOR_READY,
    WLT_NET_TOR_FAILED,
} wlt_net_tor_state_t;

typedef enum {
    WLT_NET_MAC_NEVER = 0,
    WLT_NET_MAC_ON_BOOT,
    WLT_NET_MAC_ALWAYS,
} wlt_net_mac_mode_t;

typedef struct {
    char ssid[WLT_NET_MAX_SSID];
    int8_t signal_dbm;
    uint8_t bssid[WLT_NET_MAX_BSSID];
    bool secure;
    uint16_t channel;
} wlt_net_wifi_network_t;

typedef struct {
    /* Configuration (loaded once) */
    char iface[WLT_NET_MAX_IFACE];
    char eth_iface[WLT_NET_MAX_IFACE];
    wlt_net_mac_mode_t mac_mode;
    uint16_t tor_socks_port;
    uint16_t tor_dns_port;
    bool kill_switch_enabled;
    bool wifi_auto_connect;
    uint32_t wifi_scan_interval_sec;
    bool eth_auto_config;

    /* Runtime link state */
    wlt_net_link_type_t link;
    wlt_net_tor_state_t tor_state;
    bool kill_switch_active;
    char connected_ssid[WLT_NET_MAX_SSID];
    char ip_addr[WLT_NET_MAX_IP];
    char gateway[WLT_NET_MAX_IP];
    char current_mac[WLT_NET_MAX_MAC_STR];

    /* Scan cache */
    wlt_net_wifi_network_t scan_results[WLT_NET_MAX_SCAN];
    size_t scan_count;

    /* Internal handles */
    bool running;
    bool use_nftables;
    int wpa_ctrl_fd;
} wlt_net_state_t;

/* ─── Logging ────────────────────────────────────────── */

typedef enum {
    WLT_NET_LOG_DEBUG = 0,
    WLT_NET_LOG_INFO = 1,
    WLT_NET_LOG_WARN = 2,
    WLT_NET_LOG_ERROR = 3,
    WLT_NET_LOG_FATAL = 4,
} wlt_net_log_level_t;

void wlt_net_log_init(const char *path);
void wlt_net_log_close(void);
void wlt_net_log(wlt_net_log_level_t level, const char *fmt, ...);

#define WLT_NET_LOG_DBG(fmt, ...) \
    wlt_net_log(WLT_NET_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define WLT_NET_LOG_INF(fmt, ...) \
    wlt_net_log(WLT_NET_LOG_INFO, fmt, ##__VA_ARGS__)
#define WLT_NET_LOG_WRN(fmt, ...) \
    wlt_net_log(WLT_NET_LOG_WARN, fmt, ##__VA_ARGS__)
#define WLT_NET_LOG_ERR(fmt, ...) \
    wlt_net_log(WLT_NET_LOG_ERROR, fmt, ##__VA_ARGS__)
#define WLT_NET_LOG_FTL(fmt, ...) \
    wlt_net_log(WLT_NET_LOG_FATAL, fmt, ##__VA_ARGS__)

/* ─── Public API (net.c) ─────────────────────────────── */

/**
 * Initialize WLT-Net: load config, randomize MAC (if on_boot),
 * install Tor-only firewall rules, open Tor proxy socket.
 */
wlt_net_error_t wlt_net_init(wlt_net_state_t *state,
                              const char *config_path);

/**
 * Main event loop. Polls Tor health every WLT_NET_TOR_CHECK_INTERVAL_MS.
 * On Tor disconnect → triggers kill switch in <100ms.
 * Returns when wlt_net_shutdown() is invoked or fatal error.
 */
wlt_net_error_t wlt_net_run_loop(wlt_net_state_t *state);

/**
 * Shutdown: disconnect WiFi, release kill switch, close sockets.
 */
wlt_net_error_t wlt_net_shutdown(wlt_net_state_t *state);

/**
 * Render current status (for WLT-Shell tray) as a single-line string.
 */
wlt_net_error_t wlt_net_get_status(const wlt_net_state_t *state,
                                    char *buf,
                                    size_t buf_size);

/** Check Tor SOCKS port; update state->tor_state. */
wlt_net_error_t wlt_net_tor_check_status(wlt_net_state_t *state);

/** Block ALL traffic immediately. Idempotent. */
wlt_net_error_t wlt_net_kill_switch_trigger(wlt_net_state_t *state);

/** Release kill switch (only safe once Tor is READY). */
wlt_net_error_t wlt_net_kill_switch_release(wlt_net_state_t *state);

/** Send NEWNYM to Tor control port for a fresh identity. */
wlt_net_error_t wlt_net_tor_new_identity(wlt_net_state_t *state);

/** Randomize MAC on the active interface now. */
wlt_net_error_t wlt_net_mac_randomize_now(wlt_net_state_t *state);

/** Run external command without a shell (no system() per coding style). */
wlt_net_error_t wlt_net_run_cmd(const char *path,
                                 char *const argv[],
                                 int *exit_code);

/* ─── Config (config.c) ──────────────────────────────── */

wlt_net_error_t wlt_net_config_defaults(wlt_net_state_t *state);
wlt_net_error_t wlt_net_config_load(wlt_net_state_t *state,
                                     const char *path);

/* ─── Ethernet (ethernet.c) ──────────────────────────── */

wlt_net_error_t wlt_ethernet_init(wlt_net_state_t *state);
wlt_net_error_t wlt_ethernet_configure_dhcp(wlt_net_state_t *state);
wlt_net_error_t wlt_ethernet_configure_static(wlt_net_state_t *state,
                                               const char *ip,
                                               const char *gateway,
                                               const char *netmask);
wlt_net_error_t wlt_ethernet_bring_up(const char *iface);
wlt_net_error_t wlt_ethernet_bring_down(const char *iface);
bool wlt_ethernet_link_up(const char *iface);

/* ─── Kill Switch (kill_switch.c) ────────────────────── */

wlt_net_error_t wlt_kill_switch_install(wlt_net_state_t *state);
wlt_net_error_t wlt_kill_switch_apply_block(wlt_net_state_t *state);
wlt_net_error_t wlt_kill_switch_apply_tor_only(wlt_net_state_t *state);
wlt_net_error_t wlt_kill_switch_cleanup(wlt_net_state_t *state);
bool wlt_kill_switch_nftables_available(void);

/* ─── Helpers ────────────────────────────────────────── */

const char *wlt_net_error_str(wlt_net_error_t err);
const char *wlt_net_link_str(wlt_net_link_type_t link);
const char *wlt_net_tor_state_str(wlt_net_tor_state_t tor);

#ifdef __cplusplus
}
#endif

#endif /* WLT_NET_H */
