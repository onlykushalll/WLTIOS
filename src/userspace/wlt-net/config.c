/**
 * @file config.c
 * @brief INI config parser for WLT-Net
 * @module wlt-net/config
 * @purpose Parse /etc/wlt/network.conf (tor_socks_port, kill_switch,
 *          mac_randomize, wifi, ethernet)
 * @dependencies: net.h
 * @tested_by: tests/userspace/config_test.c
 * @spec: docs/specs/wlt-net.md
 */

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ─── Helpers ────────────────────────────────────────── */

static char *
wlt_config_trim(char *str)
{
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != str)
        memmove(str, start, strlen(start) + 1);
    if (*str)
    {
        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return str;
}

static int
wlt_config_parse_kv(const char *line,
                    char *key, int key_max,
                    char *val, int val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL)
        return -1;

    int klen = (int)(eq - line);
    if (klen >= key_max)
        klen = key_max - 1;
    memcpy(key, line, (size_t)klen);
    key[klen] = '\0';
    wlt_config_trim(key);

    const char *vstart = eq + 1;
    int vlen = (int)strlen(vstart);
    if (vlen >= val_max)
        vlen = val_max - 1;
    memcpy(val, vstart, (size_t)vlen);
    val[vlen] = '\0';
    wlt_config_trim(val);

    return 0;
}

static wlt_net_mac_mode_t
wlt_config_parse_mac_mode(const char *val)
{
    if (strcmp(val, "on_boot") == 0)
        return WLT_NET_MAC_ON_BOOT;
    if (strcmp(val, "always") == 0)
        return WLT_NET_MAC_ALWAYS;
    return WLT_NET_MAC_NEVER;
}

static bool
wlt_config_parse_bool(const char *val)
{
    return (strcmp(val, "yes") == 0 ||
            strcmp(val, "true") == 0 ||
            strcmp(val, "enabled") == 0 ||
            strcmp(val, "1") == 0);
}

/* ─── Public API ─────────────────────────────────────── */

wlt_net_error_t
wlt_net_config_defaults(wlt_net_state_t *state)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    /* Network interfaces */
    strncpy(state->iface, "wlan0", WLT_NET_MAX_IFACE - 1);
    strncpy(state->eth_iface, "eth0", WLT_NET_MAX_IFACE - 1);

    /* Tor proxy — defaults from spec */
    state->tor_socks_port = WLT_NET_DEFAULT_TOR_SOCKS_PORT;
    state->tor_dns_port = WLT_NET_DEFAULT_TOR_DNS_PORT;

    /* Security defaults — fail-closed */
    state->kill_switch_enabled = true;
    state->mac_mode = WLT_NET_MAC_ON_BOOT;

    /* WiFi */
    state->wifi_auto_connect = true;
    state->wifi_scan_interval_sec = 30;

    /* Ethernet */
    state->eth_auto_config = true;

    /* Runtime */
    state->link = WLT_NET_LINK_DOWN;
    state->tor_state = WLT_NET_TOR_DISCONNECTED;
    state->wpa_ctrl_fd = -1;

    return WLT_NET_OK;
}

wlt_net_error_t
wlt_net_config_load(wlt_net_state_t *state, const char *path)
{
    if (state == NULL)
        return WLT_NET_ERR_INVALID_ARG;

    const char *cfg_path = (path != NULL) ? path : WLT_NET_CONFIG_PATH;
    FILE *f = fopen(cfg_path, "r");
    if (f == NULL)
    {
        /* Missing config is non-fatal — defaults are Tor-only fail-closed. */
        WLT_NET_LOG_WRN("Config %s not found, using defaults", cfg_path);
        return WLT_NET_OK;
    }

    char section[64] = "";
    char line[WLT_NET_MAX_LINE];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *t = wlt_config_trim(line);
        if (*t == '\0' || *t == '#' || *t == ';')
            continue;

        if (*t == '[')
        {
            char *end = strchr(t, ']');
            if (end != NULL)
            {
                *end = '\0';
                strncpy(section, t + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            continue;
        }

        char key[128];
        char val[300];
        if (wlt_config_parse_kv(t, key, sizeof(key),
                                  val, sizeof(val)) != 0)
            continue;

        if (strcmp(section, "wlt-net") == 0)
        {
            if (strcmp(key, "tor_socks_port") == 0)
                state->tor_socks_port = (uint16_t)atoi(val);
            else if (strcmp(key, "tor_dns_port") == 0)
                state->tor_dns_port = (uint16_t)atoi(val);
            else if (strcmp(key, "kill_switch") == 0)
                state->kill_switch_enabled = (strcmp(val, "enabled") == 0 ||
                                              wlt_config_parse_bool(val));
            else if (strcmp(key, "mac_randomize") == 0)
                state->mac_mode = wlt_config_parse_mac_mode(val);
        }
        else if (strcmp(section, "wifi") == 0)
        {
            if (strcmp(key, "auto_connect") == 0)
                state->wifi_auto_connect = wlt_config_parse_bool(val);
            else if (strcmp(key, "scan_interval") == 0)
                state->wifi_scan_interval_sec = (uint32_t)atoi(val);
            else if (strcmp(key, "iface") == 0)
            {
                strncpy(state->iface, val, WLT_NET_MAX_IFACE - 1);
                state->iface[WLT_NET_MAX_IFACE - 1] = '\0';
            }
        }
        else if (strcmp(section, "ethernet") == 0)
        {
            if (strcmp(key, "auto_config") == 0)
                state->eth_auto_config = wlt_config_parse_bool(val);
            else if (strcmp(key, "iface") == 0)
            {
                strncpy(state->eth_iface, val, WLT_NET_MAX_IFACE - 1);
                state->eth_iface[WLT_NET_MAX_IFACE - 1] = '\0';
            }
        }
    }

    fclose(f);

    /* Validate ranges — refuse unsafe ports. uint16_t upper bound
     * is enforced by the type, so only zero (unspecified) needs
     * guarding. */
    if (state->tor_socks_port == 0)
        state->tor_socks_port = WLT_NET_DEFAULT_TOR_SOCKS_PORT;
    if (state->tor_dns_port == 0)
        state->tor_dns_port = WLT_NET_DEFAULT_TOR_DNS_PORT;

    WLT_NET_LOG_INF("Config loaded: socks=%u dns=%u kill_switch=%s mac=%s",
                    state->tor_socks_port, state->tor_dns_port,
                    state->kill_switch_enabled ? "on" : "off",
                    state->mac_mode == WLT_NET_MAC_ALWAYS ? "always" :
                    state->mac_mode == WLT_NET_MAC_ON_BOOT ? "on_boot" : "never");
    return WLT_NET_OK;
}
