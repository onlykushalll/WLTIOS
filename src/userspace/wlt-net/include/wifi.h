/**
 * @file wifi.h
 * @brief WiFi scanning and connection via nl80211 + wpa_supplicant
 * @module wlt-net/wifi
 * @purpose Scan for networks, connect via WPA, manage supplicant state
 * @dependencies: net.h
 * @tested_by: tests/userspace/wifi_test.c
 * @spec: docs/specs/wlt-net.md
 */

#ifndef WLT_WIFI_H
#define WLT_WIFI_H

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WLT_WIFI_WPA_CTRL_DIR "/run/wpa_supplicant"
#define WLT_WIFI_MAX_NETWORKS WLT_NET_MAX_SCAN
#define WLT_WIFI_PSK_LEN 64
#define WLT_WIFI_WPA_RESP_BUF 4096
#define WLT_WIFI_WPA_TIMEOUT_MS 3000

/**
 * Initialize WiFi subsystem — detects hardware, locates interface,
 * opens wpa_supplicant control socket.
 */
wlt_net_error_t wlt_wifi_init(wlt_net_state_t *state);

/**
 * Scan for WiFi networks — populates state->scan_results.
 * Retries up to WLT_NET_SCAN_RETRIES times on failure.
 */
wlt_net_error_t wlt_wifi_scan(wlt_net_state_t *state);

/**
 * Connect to a WiFi network via wpa_supplicant.
 * @param ssid  Network SSID (UTF-8, max 32 bytes effective)
 * @param psk   Pre-shared key (NULL for open networks)
 */
wlt_net_error_t wlt_wifi_connect(wlt_net_state_t *state,
                                  const char *ssid,
                                  const char *psk);

/**
 * Disconnect from current WiFi network.
 */
wlt_net_error_t wlt_wifi_disconnect(wlt_net_state_t *state);

/**
 * Forget a saved WiFi network (removes from wpa_supplicant config).
 */
wlt_net_error_t wlt_wifi_forget(wlt_net_state_t *state, const char *ssid);

/**
 * Check if WiFi hardware is present (looks for /sys/class/net/<iface>/wireless).
 */
bool wlt_wifi_hardware_present(void);

/**
 * Find the primary WiFi interface name (e.g. "wlan0").
 * Writes interface name to buf.
 */
wlt_net_error_t wlt_wifi_find_iface(char *buf, size_t buf_size);

/**
 * Release wpa_supplicant control socket.
 */
wlt_net_error_t wlt_wifi_cleanup(wlt_net_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* WLT_WIFI_H */
