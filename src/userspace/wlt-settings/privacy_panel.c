/*
 * WLT-Settings - Privacy panel
 * ============================
 *
 * Owns WLT_CAT_PRIVACY: Tor circuit isolation, identity, telemetry
 * (hard-wired off), data retention, microphone/camera/location controls,
 * clipboard isolation and history clearing.
 *
 * Telemetry is non-negotiable on WLTIOS: the setting exists so the UI can
 * display it, but it is WLT_FLAG_READONLY and defaults to false, so it can
 * never be flipped on through this API.
 *
 * apply() pushes to wlt-privacyd / Tor controller; returns UNREACHABLE
 * when the live daemon is absent.
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
static const char *const k_circuit_modes[] = { "shared", "per-app", "per-session", NULL };

/* ------------------------------------------------------------------ */
/* Descriptor builders (privacy-local)                                */
/* ------------------------------------------------------------------ */
static void desc_init(wlt_setting_desc_t *d, const char *key, const char *desc,
                      wlt_setting_type_t type, uint32_t flags)
{
    memset(d, 0, sizeof(*d));
    strncpy(d->key, key, WLT_SETTINGS_KEY_MAX - 1);
    strncpy(d->description, desc, WLT_SETTINGS_DESC_MAX - 1);
    d->category = WLT_CAT_PRIVACY;
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
    d.min.i = lo; d.max.i = hi;
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
static wlt_settings_err_t wlt_privacy_register(wlt_settings_t *s)
{
    wlt_settings_err_t r;

    /* Tor circuit isolation */
    if ((r = reg_enum(s, "privacy.tor.circuit_mode", "Tor circuit isolation mode",
                      "per-app", k_circuit_modes, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int (s, "privacy.tor.circuits_per_app",
                      "Max isolated circuits per app", 3,
                      1, 8, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "privacy.tor.dns_over_tor", "Resolve DNS through Tor",
                      true, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Identity */
    if ((r = reg_str (s, "privacy.identity.alias", "Public alias", "",
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "privacy.identity.spoof_timezone",
                      "Spoof timezone to UTC", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Telemetry -- permanently off and read-only. */
    if ((r = reg_bool(s, "privacy.telemetry.enabled",
                      "Usage telemetry (always off)", false,
                      WLT_FLAG_READONLY)) != WLT_SETTINGS_OK) return r;

    /* Data retention */
    if ((r = reg_int (s, "privacy.data_retention.days",
                      "Days to retain logs (0 = none)", 0,
                      0, 365, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Device sensors */
    if ((r = reg_bool(s, "privacy.microphone.muted", "Mute microphone by default",
                      false, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "privacy.camera.disabled", "Disable all cameras",
                      false, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "privacy.location.enabled", "Allow location access",
                      false, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Isolation */
    if ((r = reg_bool(s, "privacy.clipboard.isolation",
                      "Isolate clipboard per application", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool(s, "privacy.history.clear_on_exit",
                      "Clear history on logout", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Cross-field validation                                             */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t check_circuit_consistency(const char *mode,
                                                    int64_t per_app)
{
    if (!mode)
        return WLT_SETTINGS_ERR_INVALID;
    if (strcmp(mode, "per-app") == 0 && per_app <= 0)
        return WLT_SETTINGS_ERR_INVALID;        /* need >=1 circuit per app */
    if (strcmp(mode, "shared") == 0 && per_app > 1)
        return WLT_SETTINGS_ERR_INVALID;        /* shared uses one circuit */
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Apply                                                              */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t wlt_privacy_apply(wlt_settings_t *s)
{
    char circuit_mode[WLT_SETTINGS_STR_MAX] = {0};
    char alias[WLT_SETTINGS_STR_MAX] = {0};
    int64_t per_app = 0, retention = 0;
    bool tor_dns = false, mic_muted = false, cam_off = false;
    bool location = false, clip_iso = false, clear_hist = false;
    bool telemetry = false;

    (void)wlt_settings_get_string(s, "privacy.tor.circuit_mode", circuit_mode, sizeof(circuit_mode));
    (void)wlt_settings_get_int   (s, "privacy.tor.circuits_per_app", &per_app);
    (void)wlt_settings_get_bool  (s, "privacy.tor.dns_over_tor", &tor_dns);
    (void)wlt_settings_get_string(s, "privacy.identity.alias", alias, sizeof(alias));
    (void)wlt_settings_get_int   (s, "privacy.data_retention.days", &retention);
    (void)wlt_settings_get_bool  (s, "privacy.microphone.muted", &mic_muted);
    (void)wlt_settings_get_bool  (s, "privacy.camera.disabled", &cam_off);
    (void)wlt_settings_get_bool  (s, "privacy.location.enabled", &location);
    (void)wlt_settings_get_bool  (s, "privacy.clipboard.isolation", &clip_iso);
    (void)wlt_settings_get_bool  (s, "privacy.history.clear_on_exit", &clear_hist);
    (void)wlt_settings_get_bool  (s, "privacy.telemetry.enabled", &telemetry);

    /* Telemetry must never be on. */
    if (telemetry)
        return WLT_SETTINGS_ERR_INVALID;

    /* Circuit mode / count consistency. */
    wlt_settings_err_t r = check_circuit_consistency(circuit_mode, per_app);
    if (r != WLT_SETTINGS_OK) return r;

    /* DNS-over-Tor is meaningless if Tor itself is off (checked in network
     * panel); here we just require a non-negative retention. */
    if (retention < 0)
        return WLT_SETTINGS_ERR_INVALID;

    /*
     * Live enactment (Tor SIGNAL NEWNYM for new identity, retention
     * pruning, sensor ACLs) would be dispatched here. Without the privacy
     * daemon reachable we report UNREACHABLE.
     */
    return WLT_SETTINGS_ERR_UNREACHABLE;
}

/* ------------------------------------------------------------------ */
/* Panel export                                                       */
/* ------------------------------------------------------------------ */
const wlt_panel_t wlt_privacy_panel = {
    .name              = "privacy",
    .category          = WLT_CAT_PRIVACY,
    .register_settings = wlt_privacy_register,
    .apply             = wlt_privacy_apply
};
