/*
 * WLT-Settings - Public API
 * =========================
 * Central system-settings key-value store for WLTIOS.
 *
 * Spec: docs/specs/wlt-settings.md (v1.0)
 *
 * The settings manager holds a registry of typed setting descriptors with
 * current values, validates every mutation, persists to per-category config
 * files under /etc/wlt/ via the pluggable config I/O backend, and broadcasts
 * change events to registered listeners (panels, compositors, services).
 *
 * This header is the ONLY public surface; panels and config_io implement
 * against it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WLT_SETTINGS_H
#define WLT_SETTINGS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Version                                                            */
/* ------------------------------------------------------------------ */
#define WLT_SETTINGS_VERSION_MAJOR 1
#define WLT_SETTINGS_VERSION_MINOR 0
#define WLT_SETTINGS_VERSION_PATCH 0
#define WLT_SETTINGS_VERSION_STRING "1.0.0"

/* ------------------------------------------------------------------ */
/* Limits                                                             */
/* ------------------------------------------------------------------ */
#define WLT_SETTINGS_KEY_MAX        128
#define WLT_SETTINGS_STR_MAX        512
#define WLT_SETTINGS_DESC_MAX       256
#define WLT_SETTINGS_MAX_ENTRIES    256
#define WLT_SETTINGS_MAX_LISTENERS  32
#define WLT_SETTINGS_MAX_PANELS     16
#define WLT_SETTINGS_ENUM_MAX       32
#ifndef WLT_SETTINGS_CONF_DIR
#define WLT_SETTINGS_CONF_DIR       "/etc/wlt"
#endif

/* ------------------------------------------------------------------ */
/* Error codes                                                        */
/* ------------------------------------------------------------------ */
typedef enum {
    WLT_SETTINGS_OK             = 0,
    WLT_SETTINGS_ERR_INVALID    = -1,   /* bad argument                */
    WLT_SETTINGS_ERR_NOTFOUND   = -2,   /* key not registered          */
    WLT_SETTINGS_ERR_TYPE       = -3,   /* value type mismatch         */
    WLT_SETTINGS_ERR_RANGE      = -4,   /* value out of min/max        */
    WLT_SETTINGS_ERR_PERM       = -5,   /* filesystem permission       */
    WLT_SETTINGS_ERR_IO         = -6,   /* generic I/O failure         */
    WLT_SETTINGS_ERR_FULL       = -7,   /* registry / listener full    */
    WLT_SETTINGS_ERR_NOMEM      = -8,   /* allocation failure          */
    WLT_SETTINGS_ERR_AUTH       = -9,   /* admin auth required/denied  */
    WLT_SETTINGS_ERR_UNREACHABLE= -10,  /* target module unreachable   */
    WLT_SETTINGS_ERR_ENUM       = -11   /* value not in enum set       */
} wlt_settings_err_t;

/* ------------------------------------------------------------------ */
/* Setting types                                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    WLT_TYPE_BOOL   = 0,
    WLT_TYPE_INT    = 1,
    WLT_TYPE_FLOAT  = 2,
    WLT_TYPE_STRING = 3,
    WLT_TYPE_ENUM   = 4
} wlt_setting_type_t;

/* ------------------------------------------------------------------ */
/* Categories                                                         */
/* ------------------------------------------------------------------ */
typedef enum {
    WLT_CAT_NETWORK  = 0,
    WLT_CAT_DISPLAY  = 1,
    WLT_CAT_SOUND    = 2,
    WLT_CAT_SECURITY = 3,
    WLT_CAT_PRIVACY  = 4,
    WLT_CAT_ACCOUNTS = 5,
    WLT_CAT_UPDATES  = 6,
    WLT_CAT_APPS     = 7,
    WLT_CAT_SYSTEM   = 8,
    WLT_CAT_ABOUT    = 9,
    WLT_CAT_COUNT
} wlt_category_t;

/* ------------------------------------------------------------------ */
/* Value union                                                        */
/* ------------------------------------------------------------------ */
typedef union {
    bool    b;
    int64_t i;
    double  f;
    char    s[WLT_SETTINGS_STR_MAX];
} wlt_value_t;

/* ------------------------------------------------------------------ */
/* Flags                                                              */
/* ------------------------------------------------------------------ */
#define WLT_FLAG_NONE           0x00u
#define WLT_FLAG_REQUIRES_AUTH  0x01u   /* needs admin to change       */
#define WLT_FLAG_READONLY       0x02u   /* display only                */
#define WLT_FLAG_HIDDEN         0x04u   /* not shown in UI             */
#define WLT_FLAG_PERSIST        0x08u   /* written to config file      */
#define WLT_FLAG_EXPERIMENTAL   0x10u   /* unstable / behind a toggle  */

/* ------------------------------------------------------------------ */
/* Setting descriptor (static schema)                                 */
/* ------------------------------------------------------------------ */
typedef struct {
    char                 key[WLT_SETTINGS_KEY_MAX];
    char                 description[WLT_SETTINGS_DESC_MAX];
    wlt_category_t       category;
    wlt_setting_type_t   type;
    uint32_t             flags;
    wlt_value_t          default_value;
    wlt_value_t          min;            /* int/float only            */
    wlt_value_t          max;            /* int/float only            */
    const char *const   *enum_values;    /* NULL-terminated (ENUM)    */
} wlt_setting_desc_t;

/* ------------------------------------------------------------------ */
/* Runtime entry                                                      */
/* ------------------------------------------------------------------ */
typedef struct {
    wlt_setting_desc_t desc;
    wlt_value_t        value;
    bool               dirty;            /* changed since last save   */
} wlt_setting_entry_t;

/* ------------------------------------------------------------------ */
/* Change events                                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    WLT_EVENT_SET      = 0,
    WLT_EVENT_REVERTED = 1,
    WLT_EVENT_SAVED    = 2,
    WLT_EVENT_LOADED   = 3,
    WLT_EVENT_RESET    = 4
} wlt_event_type_t;

typedef struct {
    const char      *key;
    wlt_event_type_t type;
    wlt_value_t      old_value;
    wlt_value_t      new_value;
} wlt_change_event_t;

/* Listener callback. */
typedef void (*wlt_listener_cb_t)(const wlt_change_event_t *evt, void *user);

/* Auth callback. Returns non-zero to GRANT the privileged operation. */
typedef int (*wlt_auth_cb_t)(const char *operation, void *user);

/* ------------------------------------------------------------------ */
/* Settings manager (opaque)                                          */
/* ------------------------------------------------------------------ */
typedef struct wlt_settings wlt_settings_t;

/* ---- Lifecycle ---- */
wlt_settings_err_t wlt_settings_create(wlt_settings_t **out);
void               wlt_settings_destroy(wlt_settings_t *s);

/* ---- Registration ---- */
wlt_settings_err_t wlt_settings_register(wlt_settings_t *s,
                                         const wlt_setting_desc_t *desc);
wlt_settings_err_t wlt_settings_unregister(wlt_settings_t *s, const char *key);

/* ---- Typed getters ---- */
wlt_settings_err_t wlt_settings_get_bool  (wlt_settings_t *s, const char *key, bool    *out);
wlt_settings_err_t wlt_settings_get_int   (wlt_settings_t *s, const char *key, int64_t *out);
wlt_settings_err_t wlt_settings_get_float (wlt_settings_t *s, const char *key, double  *out);
wlt_settings_err_t wlt_settings_get_string(wlt_settings_t *s, const char *key,
                                           char *buf, size_t len);

/* ---- Typed setters (validate + notify + mark dirty) ---- */
wlt_settings_err_t wlt_settings_set_bool  (wlt_settings_t *s, const char *key, bool        v);
wlt_settings_err_t wlt_settings_set_int   (wlt_settings_t *s, const char *key, int64_t     v);
wlt_settings_err_t wlt_settings_set_float (wlt_settings_t *s, const char *key, double      v);
wlt_settings_err_t wlt_settings_set_string(wlt_settings_t *s, const char *key, const char *v);

/* ---- Enumeration / lookup ---- */
wlt_settings_err_t wlt_settings_list_by_category(wlt_settings_t *s, wlt_category_t cat,
                                                 const wlt_setting_entry_t **entries,
                                                 size_t *count);
wlt_settings_err_t wlt_settings_find(wlt_settings_t *s, const char *key,
                                     const wlt_setting_entry_t **entry);
size_t             wlt_settings_count(const wlt_settings_t *s);

/* ---- Reset ---- */
wlt_settings_err_t wlt_settings_reset(wlt_settings_t *s, const char *key);
wlt_settings_err_t wlt_settings_reset_category(wlt_settings_t *s, wlt_category_t cat);

/* ---- Persistence (delegates to config I/O backend) ---- */
wlt_settings_err_t wlt_settings_save(wlt_settings_t *s);
wlt_settings_err_t wlt_settings_load(wlt_settings_t *s);
wlt_settings_err_t wlt_settings_save_category(wlt_settings_t *s, wlt_category_t cat);
wlt_settings_err_t wlt_settings_load_category(wlt_settings_t *s, wlt_category_t cat);

/* ---- Change notification ---- */
wlt_settings_err_t wlt_settings_add_listener   (wlt_settings_t *s,
                                                wlt_listener_cb_t cb, void *user);
wlt_settings_err_t wlt_settings_remove_listener(wlt_settings_t *s,
                                                wlt_listener_cb_t cb, void *user);

/* ---- Auth ---- */
void wlt_settings_set_auth_cb(wlt_settings_t *s, wlt_auth_cb_t cb, void *user);

/* ---- Validation (public, used by UI before commit) ---- */
wlt_settings_err_t wlt_settings_validate(wlt_settings_t *s, const char *key,
                                         const wlt_value_t *v,
                                         wlt_setting_type_t type);

/*
 * Backend hook used by config I/O implementations. Stores a value that was
 * read from persistent storage: it validates the value, copies it in,
 * emits a WLT_EVENT_LOADED event, and clears the entry's dirty flag (the
 * value now matches what is on disk). It does NOT invoke the auth callback
 * because loading is not a privileged action. Panels never call this.
 */
wlt_settings_err_t wlt_settings_load_value(wlt_settings_t *s, const char *key,
                                           const wlt_value_t *v,
                                           wlt_setting_type_t type);

/* ---- Helpers ---- */
const char *wlt_settings_err_str     (wlt_settings_err_t err);
const char *wlt_settings_category_str(wlt_category_t cat);
const char *wlt_settings_type_str    (wlt_setting_type_t type);

/* ------------------------------------------------------------------ */
/* Panels                                                             */
/* ------------------------------------------------------------------ */
/*
 * A panel owns the schema + live-apply behaviour for one category. The
 * settings manager boots by registering every built-in panel, which fills
 * the registry with descriptors. apply() pushes the current values to the
 * running module over IPC.
 */
typedef struct {
    const char        *name;
    wlt_category_t     category;
    wlt_settings_err_t (*register_settings)(wlt_settings_t *s);
    wlt_settings_err_t (*apply)(wlt_settings_t *s);
} wlt_panel_t;

wlt_settings_err_t wlt_settings_register_panel(wlt_settings_t *s,
                                               const wlt_panel_t *panel);
wlt_settings_err_t wlt_settings_apply_category(wlt_settings_t *s, wlt_category_t cat);
wlt_settings_err_t wlt_settings_apply_all(wlt_settings_t *s);

/* Built-in panels (defined in their respective .c files). */
extern const wlt_panel_t wlt_network_panel;
extern const wlt_panel_t wlt_display_panel;
extern const wlt_panel_t wlt_security_panel;
extern const wlt_panel_t wlt_privacy_panel;

/* ------------------------------------------------------------------ */
/* Config I/O backend                                                 */
/* ------------------------------------------------------------------ */
/*
 * Pluggable so tests can swap in an in-memory backend. The default
 * implementation (config_io.c) reads/writes /etc/wlt/<category>.conf as
 * INI text. It only uses the public settings API, so backends need no
 * access to manager internals.
 */
typedef struct {
    wlt_settings_err_t (*load_category)(wlt_settings_t *s, wlt_category_t cat);
    wlt_settings_err_t (*save_category)(wlt_settings_t *s, wlt_category_t cat);
} wlt_config_io_t;

extern const wlt_config_io_t wlt_default_config_io;

void wlt_settings_set_config_io(wlt_settings_t *s, const wlt_config_io_t *io);

/* Convenience: map a category to its on-disk filename stem. */
const char *wlt_config_category_filename(wlt_category_t cat);

#ifdef __cplusplus
}
#endif

#endif /* WLT_SETTINGS_H */
