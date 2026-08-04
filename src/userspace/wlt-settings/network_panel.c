/*
 * WLT-Settings - Network panel
 * ============================
 *
 * Owns the schema and live-apply behaviour for WLT_CAT_NETWORK:
 *   - WiFi (enable, SSID, auto-connect)
 *   - Ethernet
 *   - Tor transport (SOCKS / control ports)
 *   - Kill switch (requires admin)
 *   - MAC randomization
 *   - Proxy
 *
 * apply() would normally push the new values to wlt-networkd over the
 * WLT-Init IPC bus. That bus is not available in this build, so apply()
 * performs final cross-field validation and reports UNREACHABLE for any
 * value it cannot confirm was accepted, mirroring the real contract.
 *
 * Spec: docs/specs/wlt-settings.md
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "settings.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Enum tables                                                        */
/* ------------------------------------------------------------------ */
static const char *const k_killswitch_modes[]   = { "off", "lan-only", "vpn-only", NULL };
static const char *const k_mac_modes[]          = { "disabled", "per-connection", "stable", NULL };
static const char *const k_dns_providers[]      = { "default", "tor", "cloudflare", "quad9", NULL };

/* ------------------------------------------------------------------ */
/* Small descriptor builder                                           */
/* ------------------------------------------------------------------ */
static void desc_init(wlt_setting_desc_t *d, const char *key, const char *desc,
                      wlt_setting_type_t type, uint32_t flags)
{
    memset(d, 0, sizeof(*d));
    strncpy(d->key, key, WLT_SETTINGS_KEY_MAX - 1);
    strncpy(d->description, desc, WLT_SETTINGS_DESC_MAX - 1);
    d->category = WLT_CAT_NETWORK;
    d->type     = type;
    d->flags    = flags;
}

static wlt_settings_err_t reg_bool(wlt_settings_t *s, const char *key,
                                   const char *desc, bool def, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_BOOL, flags);
    d.default_value.b = def;
    return wlt_settings_register(s, &d);
}

static wlt_settings_err_t reg_int(wlt_settings_t *s, const char *key,
                                  const char *desc, int64_t def,
                                  int64_t lo, int64_t hi, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_INT, flags);
    d.default_value.i = def;
    d.min.i = lo;
    d.max.i = hi;
    return wlt_settings_register(s, &d);
}

static wlt_settings_err_t reg_str(wlt_settings_t *s, const char *key,
                                  const char *desc, const char *def, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_STRING, flags);
    strncpy(d.default_value.s, def ? def : "", WLT_SETTINGS_STR_MAX - 1);
    return wlt_settings_register(s, &d);
}

static wlt_settings_err_t reg_enum(wlt_settings_t *s, const char *key,
                                   const char *desc, const char *def,
                                   const char *const *vals, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_ENUM, flags);
    strncpy(d.default_value.s, def, WLT_SETTINGS_STR_MAX - 1);
    d.enum_values = vals;
    return wlt_settings_register(s, &d);
}

/* ------------------------------------------------------------------ */
/* Schema                                                             */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t wlt_network_register(wlt_settings_t *s)
{
    wlt_settings_err_t r;

    /* WiFi */
    if ((r = reg_bool(s, "network.wifi.enabled", "Enable WiFi", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_str(s, "network.wifi.ssid", "Connected WiFi SSID", "",
                     WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "network.wifi.auto_connect", "Auto-connect to known networks",
                      true, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Ethernet */
    if ((r = reg_bool(s, "network.ethernet.enabled", "Enable Ethernet", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Tor transport
     * [fix] default was false, contradicting this project's own
     * stated P0 requirement that all traffic routes through Tor by
     * default. A fresh install with no persisted config previously
     * had Tor off by default in this schema. */
    if ((r = reg_bool(s, "network.tor.enabled", "Route traffic through Tor",
                      true, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int(s, "network.tor.socks_port", "Tor SOCKS5 port", 9050,
                     1024, 65535, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int(s, "network.tor.control_port", "Tor control port", 9051,
                     1024, 65535, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Kill switch (privileged)
     * [fix] default was false here, inconsistent with wlt-net's own
     * actual runtime default (kill_switch_enabled = true in
     * config.c's fail-closed defaults). */
    if ((r = reg_bool(s, "network.kill_switch.enabled",
                      "Block all non-Tor traffic", true,
                      WLT_FLAG_PERSIST | WLT_FLAG_REQUIRES_AUTH)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum(s, "network.kill_switch.mode", "Kill switch mode",
                      "off", k_killswitch_modes,
                      WLT_FLAG_PERSIST | WLT_FLAG_REQUIRES_AUTH)) != WLT_SETTINGS_OK) return r;

    /* MAC randomization */
    if ((r = reg_enum(s, "network.mac.randomization", "MAC address randomization",
                      "per-connection", k_mac_modes, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* DNS */
    if ((r = reg_enum(s, "network.dns.provider", "DNS resolver", "default",
                      k_dns_providers, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Proxy */
    if ((r = reg_bool(s, "network.proxy.enabled", "Enable manual proxy",
                      false, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_str(s, "network.proxy.url", "Proxy URL (scheme://host:port)", "",
                     WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Cross-field validation                                             */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t check_proxy_url(const char *url)
{
    if (!url || url[0] == '\0')
        return WLT_SETTINGS_ERR_INVALID;
    /* Accept socks5://, http://, https:// schemes only. */
    if (strncmp(url, "socks5://", 9) == 0) return WLT_SETTINGS_OK;
    if (strncmp(url, "http://",   7) == 0) return WLT_SETTINGS_OK;
    if (strncmp(url, "https://",  8) == 0) return WLT_SETTINGS_OK;
    return WLT_SETTINGS_ERR_INVALID;
}

static wlt_settings_err_t check_tor_ports(int64_t socks, int64_t ctrl)
{
    if (socks == ctrl)
        return WLT_SETTINGS_ERR_INVALID;       /* ports must differ */
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Apply (push to wlt-networkd via IPC)                               */
/* ------------------------------------------------------------------ */
/*
 * In production this opens the WLT-Init IPC socket and sends a set of
 * (key,value) deltas to wlt-networkd, which ACKs each one. Here we run the
 * final consistency checks and return UNREACHABLE if the (absent) daemon
 * would have been needed to enact a live change.
 */
static wlt_settings_err_t wlt_network_apply(wlt_settings_t *s)
{
    bool tor_enabled = false;
    bool wifi_enabled = false;
    bool eth_enabled = false;
    bool kill = false;
    bool proxy_enabled = false;
    char proxy_url[WLT_SETTINGS_STR_MAX] = {0};
    char ks_mode[WLT_SETTINGS_STR_MAX] = {0};
    int64_t socks_port = 0, ctrl_port = 0;

    (void)wlt_settings_get_bool(s, "network.tor.enabled", &tor_enabled);
    (void)wlt_settings_get_bool(s, "network.wifi.enabled", &wifi_enabled);
    (void)wlt_settings_get_bool(s, "network.ethernet.enabled", &eth_enabled);
    (void)wlt_settings_get_bool(s, "network.kill_switch.enabled", &kill);
    (void)wlt_settings_get_bool(s, "network.proxy.enabled", &proxy_enabled);
    (void)wlt_settings_get_string(s, "network.proxy.url", proxy_url, sizeof(proxy_url));
    (void)wlt_settings_get_string(s, "network.kill_switch.mode", ks_mode, sizeof(ks_mode));
    (void)wlt_settings_get_int(s, "network.tor.socks_port", &socks_port);
    (void)wlt_settings_get_int(s, "network.tor.control_port", &ctrl_port);

    /* Need at least one transport if Tor is off. */
    if (!tor_enabled && !wifi_enabled && !eth_enabled)
        return WLT_SETTINGS_ERR_INVALID;

    /* Tor port sanity. */
    if (tor_enabled) {
        wlt_settings_err_t r = check_tor_ports(socks_port, ctrl_port);
        if (r != WLT_SETTINGS_OK) return r;
    }

    /* Kill switch only meaningful when Tor is on. */
    if (kill && !tor_enabled && strcmp(ks_mode, "off") != 0)
        return WLT_SETTINGS_ERR_INVALID;

    /* Proxy URL must be well-formed when enabled. */
    if (proxy_enabled) {
        wlt_settings_err_t r = check_proxy_url(proxy_url);
        if (r != WLT_SETTINGS_OK) return r;
    }

    /*
     * IPC dispatch would happen here. Without the daemon we cannot confirm
     * live enactment, so report UNREACHABLE for any setting that actually
     * changed the runtime state. Callers that only persisted (no live
     * change) treat UNREACHABLE as non-fatal.
     */
    return WLT_SETTINGS_ERR_UNREACHABLE;
}

/* ------------------------------------------------------------------ */
/* Panel export                                                       */
/* ------------------------------------------------------------------ */
const wlt_panel_t wlt_network_panel = {
    .name              = "network",
    .category          = WLT_CAT_NETWORK,
    .register_settings = wlt_network_register,
    .apply             = wlt_network_apply
};
