/**
 * @file mac.h
 * @brief MAC address randomization
 * @module wlt-net/mac
 * @purpose Generate locally-administered MAC addresses, apply them with
 *          iproute2. Used on boot and on every WiFi connect.
 * @dependencies: net.h
 * @tested_by: tests/userspace/mac_test.c
 * @spec: docs/specs/wlt-net.md
 */

#ifndef WLT_MAC_H
#define WLT_MAC_H

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WLT_MAC_LEN 6
#define WLT_MAC_STR_LEN 18  /* "xx:xx:xx:xx:xx:xx" + NUL */
#define WLT_MAC_OUI_LOCALLY_ADMIN 0x02  /* Locally-administered bit */
#define WLT_MAC_OUI_MULTICAST 0x01     /* Multicast bit (must stay 0) */
#define WLT_MAC_URANDOM_PATH "/dev/urandom"

/**
 * Generate a random locally-administered MAC address.
 * Uses /dev/urandom for entropy. Sets the locally-administered bit
 * and clears the multicast bit so the address is unicast.
 */
wlt_net_error_t wlt_mac_generate(uint8_t mac[WLT_MAC_LEN]);

/**
 * Format MAC bytes as "xx:xx:xx:xx:xx:xx".
 */
wlt_net_error_t wlt_mac_format(const uint8_t mac[WLT_MAC_LEN],
                                char *buf,
                                size_t buf_size);

/**
 * Parse MAC string "xx:xx:xx:xx:xx:xx" to bytes.
 */
wlt_net_error_t wlt_mac_parse(const char *str,
                               uint8_t mac[WLT_MAC_LEN]);

/**
 * Read current MAC of interface via /sys/class/net/<iface>/address.
 */
wlt_net_error_t wlt_mac_get(const char *iface,
                             uint8_t mac[WLT_MAC_LEN]);

/**
 * Set MAC of interface using `ip link set address`.
 * Brings interface down, sets MAC, brings it back up.
 */
wlt_net_error_t wlt_mac_set(const char *iface,
                             const uint8_t mac[WLT_MAC_LEN]);

/**
 * Randomize MAC of interface (generate + set).
 */
wlt_net_error_t wlt_mac_randomize(const char *iface);

#ifdef __cplusplus
}
#endif

#endif /* WLT_MAC_H */
