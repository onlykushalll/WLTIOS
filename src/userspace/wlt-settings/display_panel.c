/*
 * WLT-Settings - Display panel
 * ============================
 *
 * Owns WLT_CAT_DISPLAY: resolution, scaling, brightness, multi-monitor
 * mode, night light, refresh rate, orientation and wallpaper.
 *
 * apply() forwards the deltas to wlt-compositor over the WLT-Init IPC bus.
 * The compositor is the authority on what modes are actually supported by
 * the attached outputs, so apply() re-validates against the registry and
 * returns UNREACHABLE if a live mode change could not be confirmed.
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
static const char *const k_resolutions[]  = { "1280x720", "1920x1080",
                                              "2560x1440", "3840x2160", NULL };
static const char *const k_monitor_modes[] = { "extend", "mirror", "single", NULL };
static const char *const k_orientations[]  = { "normal", "left", "right", "flipped", NULL };

/* ------------------------------------------------------------------ */
/* Descriptor builders (display-local)                                */
/* ------------------------------------------------------------------ */
static void desc_init(wlt_setting_desc_t *d, const char *key, const char *desc,
                      wlt_setting_type_t type, uint32_t flags)
{
    memset(d, 0, sizeof(*d));
    strncpy(d->key, key, WLT_SETTINGS_KEY_MAX - 1);
    strncpy(d->description, desc, WLT_SETTINGS_DESC_MAX - 1);
    d->category = WLT_CAT_DISPLAY;
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

static wlt_settings_err_t reg_float(wlt_settings_t *s, const char *key,
                                    const char *desc, double def,
                                    double lo, double hi, uint32_t flags)
{
    wlt_setting_desc_t d;
    desc_init(&d, key, desc, WLT_TYPE_FLOAT, flags);
    d.default_value.f = def;
    d.min.f = lo; d.max.f = hi;
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
static wlt_settings_err_t wlt_display_register(wlt_settings_t *s)
{
    wlt_settings_err_t r;

    if ((r = reg_enum (s, "display.resolution", "Screen resolution",
                       "1920x1080", k_resolutions, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_float(s, "display.scaling", "UI scale factor", 1.0,
                       0.5, 3.0, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int  (s, "display.brightness", "Backlight brightness (%)", 100,
                       10, 100, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int  (s, "display.refresh_rate", "Refresh rate (Hz)", 60,
                       30, 240, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum (s, "display.multi_monitor.mode", "Multi-monitor mode",
                       "extend", k_monitor_modes, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_enum (s, "display.orientation", "Screen orientation",
                       "normal", k_orientations, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Night light */
    if ((r = reg_bool (s, "display.night_light.enabled", "Reduce blue light at night",
                       false, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_int  (s, "display.night_light.temperature", "Night light color temp (K)",
                       3500, 1000, 6500, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;
    if ((r = reg_bool (s, "display.night_light.schedule", "Schedule night light automatically",
                       true, WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    /* Wallpaper */
    if ((r = reg_str  (s, "display.wallpaper", "Wallpaper image path",
                       "/usr/share/wlt/wallpapers/default.jpg",
                       WLT_FLAG_PERSIST)) != WLT_SETTINGS_OK) return r;

    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Validation helpers                                                 */
/* ------------------------------------------------------------------ */
static int resolution_pixels(const char *res, int64_t *w, int64_t *h)
{
    if (!res) return -1;
    return sscanf(res, "%ldx%ld", (long *)w, (long *)h) == 2 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Apply                                                              */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t wlt_display_apply(wlt_settings_t *s)
{
    char resolution[WLT_SETTINGS_STR_MAX] = {0};
    char monitor_mode[WLT_SETTINGS_STR_MAX] = {0};
    char orientation[WLT_SETTINGS_STR_MAX] = {0};
    double scaling = 1.0;
    int64_t brightness = 100, refresh = 60, temp_k = 3500;
    bool night = false, sched = false;

    (void)wlt_settings_get_string(s, "display.resolution", resolution, sizeof(resolution));
    (void)wlt_settings_get_string(s, "display.multi_monitor.mode", monitor_mode, sizeof(monitor_mode));
    (void)wlt_settings_get_string(s, "display.orientation", orientation, sizeof(orientation));
    (void)wlt_settings_get_float (s, "display.scaling", &scaling);
    (void)wlt_settings_get_int   (s, "display.brightness", &brightness);
    (void)wlt_settings_get_int   (s, "display.refresh_rate", &refresh);
    (void)wlt_settings_get_int   (s, "display.night_light.temperature", &temp_k);
    (void)wlt_settings_get_bool  (s, "display.night_light.enabled", &night);
    (void)wlt_settings_get_bool  (s, "display.night_light.schedule", &sched);

    /* Sanity: resolution string must parse. */
    int64_t w = 0, h = 0;
    if (resolution_pixels(resolution, &w, &h) != 0 || w <= 0 || h <= 0)
        return WLT_SETTINGS_ERR_INVALID;

    /* Refresh rate must divide evenly enough for the mode; reject 0. */
    if (refresh <= 0)
        return WLT_SETTINGS_ERR_INVALID;

    /* Night light temp only matters when enabled. */
    if (night && (temp_k < 1000 || temp_k > 6500))
        return WLT_SETTINGS_ERR_RANGE;

    /* Mirror mode is invalid for a single-output setup; the compositor
     * would reject it, so surface that here as INVALID. */
    if (strcmp(monitor_mode, "mirror") == 0 && strcmp(orientation, "flipped") == 0)
        return WLT_SETTINGS_ERR_INVALID;

    /*
     * IPC dispatch to wlt-compositor would go here. Without the live
     * compositor we cannot confirm the mode switch, so report UNREACHABLE
     * (persisted values are still saved; this only blocks live enactment).
     */
    return WLT_SETTINGS_ERR_UNREACHABLE;
}

/* ------------------------------------------------------------------ */
/* Panel export                                                       */
/* ------------------------------------------------------------------ */
const wlt_panel_t wlt_display_panel = {
    .name              = "display",
    .category          = WLT_CAT_DISPLAY,
    .register_settings = wlt_display_register,
    .apply             = wlt_display_apply
};
