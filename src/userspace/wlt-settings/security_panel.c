/*
 * WLT-Settings - Security panel
 * =============================
 *
 * Owns WLT_CAT_SECURITY: disk encryption, firewall, sandboxing, verified
 * boot, screen lock and USB guard.
 *
 * Every setting here is privileged: disk encryption and firewall changes
 * carry WLT_FLAG_REQUIRES_AUTH and must pass the auth callback; verified
 * boot state is hardware-reported and exposed read-only.
 *
 * apply() pushes deltas to the kernel-level services (cryptsetup,
 * nftables, the seccomp/landlock sandbox broker). Without those daemons
 * reachable it returns UNREACHABLE.
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
static const char *const k_enc_algos[]   = { "aes-xts-256", "aes-xts-512",
                                             "chacha20", NULL };
static const char *const k_fw_policies[] = { "default-allow", "default-deny", NULL };
static const char *const k_sandbox_lvl[] = { "off", "permissive", "enforcing", NULL };

/* ------------------------------------------------------------------ */
/* Descriptor builders (security-local)                               */
/* ------------------------------------------------------------------ */
static void desc_init(wlt_setting_desc_t *d, const char *key, const char *desc,
                      wlt_setting_type_t type, uint32_t flags)
{
    memset(d, 0, sizeof(*d));
    strncpy(d->key, key, WLT_SETTINGS_KEY_MAX - 1);
    strncpy(d->description, desc, WLT_SETTINGS_DESC_MAX - 1);
    d->category = WLT_CAT_SECURITY;
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

static wlt_settings_err_t reg_str(wlt_settings_t *s, const char *key,
                                  const char *desc, const char *def, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_STRING, flags);
    strncpy(d.default_value.s, def ? def : "", WLT_SETTINGS_STR_MAX - 1);
    return wlt_settings_register(s, &d);
}

/* ------------------------------------------------------------------ */
/* Schema                                                             */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t wlt_security_register(wlt_settings_t *s)
{
    wlt_settings_err_t r;
    const uint32_t AUTH = WLT_FLAG_REQUIRES_AUTH | WLT_FLAG_PERSIST;

    /* Disk encryption */
    if ((r = reg_bool(s, "security.disk_encryption.enabled",
                      "Full-disk encryption active", true,
                      AUTH | WLT_FLAG_READONLY)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum(s, "security.disk_encryption.algorithm",
                      "Encryption algorithm", "aes-xts-256",
                      k_enc_algos, AUTH)) != WLT_SETTINGS_OK) return r;

    /* Firewall */
    if ((r = reg_bool(s, "security.firewall.enabled",
                      "Enable host firewall", true, AUTH)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum(s, "security.firewall.policy",
                      "Default firewall policy", "default-deny",
                      k_fw_policies, AUTH)) != WLT_SETTINGS_OK) return r;

    /* Sandbox */
    if ((r = reg_bool(s, "security.sandbox.enabled",
                      "Sandbox untrusted apps", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum(s, "security.sandbox.level", "Sandbox enforcement level",
                      "enforcing", k_sandbox_lvl, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Verified boot (hardware-reported, read-only) */
    if ((r = reg_bool(s, "security.verified_boot.enabled",
                      "Verified boot active", false,
                      WLT_FLAG_READONLY)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_str (s, "security.verified_boot.state",
                      "Verified boot status", "unknown",
                      WLT_FLAG_READONLY)) != WLT_SETTINGS_OK) return r;

    /* Screen lock */
    if ((r = reg_bool(s, "security.screen_lock.enabled",
                      "Lock screen when idle", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int (s, "security.screen_lock.timeout",
                      "Idle lock timeout (seconds)", 300,
                      30, 3600, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* USB guard */
    if ((r = reg_bool(s, "security.usb.guard",
                      "Block unrecognised USB devices", true,
                      WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Cross-field consistency                                            */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t check_sandbox_level(bool enabled, const char *level)
{
    if (!enabled) {
        if (level && strcmp(level, "enforcing") == 0)
            return WLT_SETTINGS_ERR_INVALID;   /* can't enforce when disabled */
        return WLT_SETTINGS_OK;
    }
    if (!level || strcmp(level, "off") == 0)
        return WLT_SETTINGS_ERR_INVALID;       /* enabled implies non-off */
    return WLT_SETTINGS_OK;
}

static wlt_settings_err_t check_firewall(bool enabled, const char *policy)
{
    if (!enabled)
        return WLT_SETTINGS_OK;
    if (!policy)
        return WLT_SETTINGS_ERR_INVALID;
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Apply                                                              */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t wlt_security_apply(wlt_settings_t *s)
{
    bool enc = false, fw = false, sb = false, screen_lock = false, usb_guard = false;
    char enc_algo[WLT_SETTINGS_STR_MAX] = {0};
    char fw_policy[WLT_SETTINGS_STR_MAX] = {0};
    char sb_level[WLT_SETTINGS_STR_MAX] = {0};
    int64_t lock_timeout = 0;

    (void)wlt_settings_get_bool  (s, "security.disk_encryption.enabled", &enc);
    (void)wlt_settings_get_string(s, "security.disk_encryption.algorithm", enc_algo, sizeof(enc_algo));
    (void)wlt_settings_get_bool  (s, "security.firewall.enabled", &fw);
    (void)wlt_settings_get_string(s, "security.firewall.policy", fw_policy, sizeof(fw_policy));
    (void)wlt_settings_get_bool  (s, "security.sandbox.enabled", &sb);
    (void)wlt_settings_get_string(s, "security.sandbox.level", sb_level, sizeof(sb_level));
    (void)wlt_settings_get_bool  (s, "security.screen_lock.enabled", &screen_lock);
    (void)wlt_settings_get_int   (s, "security.screen_lock.timeout", &lock_timeout);
    (void)wlt_settings_get_bool  (s, "security.usb.guard", &usb_guard);

    /* Sandbox level / enabled consistency. */
    wlt_settings_err_t r = check_sandbox_level(sb, sb_level);
    if (r != WLT_SETTINGS_OK) return r;

    /* Firewall consistency. */
    r = check_firewall(fw, fw_policy);
    if (r != WLT_SETTINGS_OK) return r;

    /* Screen lock timeout must be positive when enabled. */
    if (screen_lock && lock_timeout <= 0)
        return WLT_SETTINGS_ERR_INVALID;

    /*
     * Privileged enactment (cryptsetup rekey, nftables reload, sandbox
     * broker reconfigure) happens here over IPC. Those services are not
     * reachable in this build, so report UNREACHABLE.
     */
    return WLT_SETTINGS_ERR_UNREACHABLE;
}

/* ------------------------------------------------------------------ */
/* Panel export                                                       */
/* ------------------------------------------------------------------ */
const wlt_panel_t wlt_security_panel = {
    .name              = "security",
    .category          = WLT_CAT_SECURITY,
    .register_settings = wlt_security_register,
    .apply             = wlt_security_apply
};
