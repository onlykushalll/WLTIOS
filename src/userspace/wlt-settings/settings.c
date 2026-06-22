/*
 * WLT-Settings - Core settings manager
 * ====================================
 *
 * Implements the in-memory key-value store declared in settings.h:
 *   - typed registry of setting descriptors
 *   - validated, atomic get/set with change notification
 *   - reset to defaults
 *   - persistence delegation to the config I/O backend
 *   - panel registration + live apply
 *
 * The store is intentionally a fixed-capacity array (no dynamic per-key
 * allocation) so lookups are O(n) but allocation-failure-free after create.
 *
 * Spec: docs/specs/wlt-settings.md
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Manager structure                                                  */
/* ------------------------------------------------------------------ */
struct wlt_settings {
    wlt_setting_entry_t  entries[WLT_SETTINGS_MAX_ENTRIES];
    size_t               count;

    const wlt_panel_t   *panels[WLT_SETTINGS_MAX_PANELS];
    size_t               panel_count;

    struct {
        wlt_listener_cb_t cb;
        void             *user;
    } listeners[WLT_SETTINGS_MAX_LISTENERS];
    size_t               listener_count;

    wlt_auth_cb_t        auth_cb;
    void                *auth_user;

    const wlt_config_io_t *io;   /* defaults to &wlt_default_config_io */
};

/* ------------------------------------------------------------------ */
/* Internal helpers                                                   */
/* ------------------------------------------------------------------ */
static wlt_setting_entry_t *find_entry(wlt_settings_t *s, const char *key)
{
    if (!s || !key)
        return NULL;
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->entries[i].desc.key, key) == 0)
            return &s->entries[i];
    }
    return NULL;
}

static const wlt_setting_entry_t *find_entry_const(const wlt_settings_t *s,
                                                   const char *key)
{
    if (!s || !key)
        return NULL;
    for (size_t i = 0; i < s->count; i++) {
        if (strcmp(s->entries[i].desc.key, key) == 0)
            return &s->entries[i];
    }
    return NULL;
}

static void copy_value(wlt_value_t *dst, const wlt_value_t *src,
                       wlt_setting_type_t type)
{
    /*
     * A union assignment copies every byte of the underlying storage. For
     * string/enum values the source is always NUL-terminated within
     * WLT_SETTINGS_STR_MAX, so the terminator is preserved; for scalar
     * types only the active member matters and the rest is harmless. This
     * avoids any truncation UB that a strncpy between same-sized buffers
     * would imply.
     */
    (void)type;
    *dst = *src;
}

static bool values_equal(const wlt_value_t *a, const wlt_value_t *b,
                         wlt_setting_type_t type)
{
    switch (type) {
    case WLT_TYPE_BOOL:  return a->b == b->b;
    case WLT_TYPE_INT:   return a->i == b->i;
    case WLT_TYPE_FLOAT: return a->f == b->f;
    case WLT_TYPE_STRING:
    case WLT_TYPE_ENUM:  return strcmp(a->s, b->s) == 0;
    default:             return false;
    }
}

static void notify(wlt_settings_t *s, const char *key, wlt_event_type_t type,
                   const wlt_value_t *oldv, const wlt_value_t *newv)
{
    if (!s)
        return;
    wlt_change_event_t evt;
    evt.key       = key;
    evt.type      = type;
    if (oldv) evt.old_value = *oldv; else memset(&evt.old_value, 0, sizeof(evt.old_value));
    if (newv) evt.new_value = *newv; else memset(&evt.new_value, 0, sizeof(evt.new_value));

    for (size_t i = 0; i < s->listener_count; i++) {
        if (s->listeners[i].cb)
            s->listeners[i].cb(&evt, s->listeners[i].user);
    }
}

/* Validate a candidate value against a descriptor. */
static wlt_settings_err_t validate_against(const wlt_setting_desc_t *d,
                                           const wlt_value_t *v,
                                           wlt_setting_type_t type)
{
    if (d->type != type)
        return WLT_SETTINGS_ERR_TYPE;

    switch (type) {
    case WLT_TYPE_BOOL:
        return WLT_SETTINGS_OK;          /* any bool is valid */

    case WLT_TYPE_INT:
        if (v->i < d->min.i || v->i > d->max.i)
            return WLT_SETTINGS_ERR_RANGE;
        return WLT_SETTINGS_OK;

    case WLT_TYPE_FLOAT:
        if (v->f < d->min.f || v->f > d->max.f)
            return WLT_SETTINGS_ERR_RANGE;
        return WLT_SETTINGS_OK;

    case WLT_TYPE_STRING:
        if (v->s[0] == '\0' && (d->flags & WLT_FLAG_PERSIST))
            return WLT_SETTINGS_ERR_INVALID;   /* empty not allowed if persisted */
        return WLT_SETTINGS_OK;

    case WLT_TYPE_ENUM:
        if (!d->enum_values)
            return WLT_SETTINGS_ERR_INVALID;
        for (size_t i = 0; d->enum_values[i] != NULL; i++) {
            if (strcmp(d->enum_values[i], v->s) == 0)
                return WLT_SETTINGS_OK;
        }
        return WLT_SETTINGS_ERR_ENUM;

    default:
        return WLT_SETTINGS_ERR_INVALID;
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_create(wlt_settings_t **out)
{
    if (!out)
        return WLT_SETTINGS_ERR_INVALID;
    wlt_settings_t *s = (wlt_settings_t *)calloc(1, sizeof(*s));
    if (!s)
        return WLT_SETTINGS_ERR_NOMEM;
    s->io = &wlt_default_config_io;
    *out = s;
    return WLT_SETTINGS_OK;
}

void wlt_settings_destroy(wlt_settings_t *s)
{
    if (!s)
        return;
    /* Zeroise so secrets in string values do not linger in freed memory. */
    memset(s, 0, sizeof(*s));
    free(s);
}

/* ------------------------------------------------------------------ */
/* Registration                                                       */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_register(wlt_settings_t *s,
                                         const wlt_setting_desc_t *desc)
{
    if (!s || !desc)
        return WLT_SETTINGS_ERR_INVALID;
    if (desc->key[0] == '\0')
        return WLT_SETTINGS_ERR_INVALID;
    if (s->count >= WLT_SETTINGS_MAX_ENTRIES)
        return WLT_SETTINGS_ERR_FULL;
    if (find_entry(s, desc->key))
        return WLT_SETTINGS_ERR_INVALID;       /* duplicate key */

    wlt_setting_entry_t *e = &s->entries[s->count];
    memset(e, 0, sizeof(*e));
    e->desc = *desc;
    copy_value(&e->value, &desc->default_value, desc->type);
    e->dirty = false;
    s->count++;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_unregister(wlt_settings_t *s, const char *key)
{
    wlt_setting_entry_t *e = find_entry(s, key);
    if (!e)
        return WLT_SETTINGS_ERR_NOTFOUND;
    /* Compact the array. */
    size_t idx = (size_t)(e - s->entries);
    memmove(&s->entries[idx], &s->entries[idx + 1],
            (s->count - idx - 1) * sizeof(s->entries[0]));
    s->count--;
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Getters                                                            */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_get_bool(wlt_settings_t *s, const char *key,
                                         bool *out)
{
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (e->desc.type != WLT_TYPE_BOOL) return WLT_SETTINGS_ERR_TYPE;
    *out = e->value.b;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_get_int(wlt_settings_t *s, const char *key,
                                        int64_t *out)
{
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (e->desc.type != WLT_TYPE_INT) return WLT_SETTINGS_ERR_TYPE;
    *out = e->value.i;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_get_float(wlt_settings_t *s, const char *key,
                                          double *out)
{
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (e->desc.type != WLT_TYPE_FLOAT) return WLT_SETTINGS_ERR_TYPE;
    *out = e->value.f;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_get_string(wlt_settings_t *s, const char *key,
                                           char *buf, size_t len)
{
    if (!buf || len == 0)
        return WLT_SETTINGS_ERR_INVALID;
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (e->desc.type != WLT_TYPE_STRING && e->desc.type != WLT_TYPE_ENUM)
        return WLT_SETTINGS_ERR_TYPE;
    strncpy(buf, e->value.s, len - 1);
    buf[len - 1] = '\0';
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Generic setter core                                                */
/* ------------------------------------------------------------------ */
static wlt_settings_err_t set_value(wlt_settings_t *s, const char *key,
                                    const wlt_value_t *v,
                                    wlt_setting_type_t type)
{
    wlt_setting_entry_t *e = find_entry(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (e->desc.flags & WLT_FLAG_READONLY)
        return WLT_SETTINGS_ERR_PERM;

    wlt_settings_err_t vr = validate_against(&e->desc, v, type);
    if (vr != WLT_SETTINGS_OK)
        return vr;

    /* Auth gate for privileged settings. */
    if ((e->desc.flags & WLT_FLAG_REQUIRES_AUTH) && s->auth_cb) {
        char op[WLT_SETTINGS_KEY_MAX + 32];
        snprintf(op, sizeof(op), "set:%s", key);
        if (!s->auth_cb(op, s->auth_user))
            return WLT_SETTINGS_ERR_AUTH;
    }

    /* No-op if unchanged. */
    if (values_equal(&e->value, v, type))
        return WLT_SETTINGS_OK;

    wlt_value_t old = e->value;
    copy_value(&e->value, v, type);
    e->dirty = true;
    notify(s, key, WLT_EVENT_SET, &old, &e->value);
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_set_bool(wlt_settings_t *s, const char *key, bool v)
{
    wlt_value_t vv; vv.b = v;
    return set_value(s, key, &vv, WLT_TYPE_BOOL);
}

wlt_settings_err_t wlt_settings_set_int(wlt_settings_t *s, const char *key, int64_t v)
{
    wlt_value_t vv; vv.i = v;
    return set_value(s, key, &vv, WLT_TYPE_INT);
}

wlt_settings_err_t wlt_settings_set_float(wlt_settings_t *s, const char *key, double v)
{
    wlt_value_t vv; vv.f = v;
    return set_value(s, key, &vv, WLT_TYPE_FLOAT);
}

wlt_settings_err_t wlt_settings_set_string(wlt_settings_t *s, const char *key,
                                           const char *v)
{
    if (!v)
        return WLT_SETTINGS_ERR_INVALID;
    wlt_value_t vv;
    strncpy(vv.s, v, WLT_SETTINGS_STR_MAX - 1);
    vv.s[WLT_SETTINGS_STR_MAX - 1] = '\0';
    /* Determine target type so enum/string share a path. */
    wlt_setting_entry_t *e = find_entry(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    return set_value(s, key, &vv, e->desc.type);
}

/* ------------------------------------------------------------------ */
/* Enumeration / lookup                                               */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_list_by_category(wlt_settings_t *s,
                                                 wlt_category_t cat,
                                                 const wlt_setting_entry_t **entries,
                                                 size_t *count)
{
    if (!s || !entries || !count)
        return WLT_SETTINGS_ERR_INVALID;
    if (cat < 0 || cat >= WLT_CAT_COUNT)
        return WLT_SETTINGS_ERR_INVALID;

    *entries = s->entries;          /* contiguous array; caller filters */
    *count   = 0;
    for (size_t i = 0; i < s->count; i++)
        if (s->entries[i].desc.category == cat)
            (*count)++;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_find(wlt_settings_t *s, const char *key,
                                     const wlt_setting_entry_t **entry)
{
    if (!entry)
        return WLT_SETTINGS_ERR_INVALID;
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    *entry = e;
    return WLT_SETTINGS_OK;
}

size_t wlt_settings_count(const wlt_settings_t *s)
{
    return s ? s->count : 0;
}

/* ------------------------------------------------------------------ */
/* Reset                                                              */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_reset(wlt_settings_t *s, const char *key)
{
    wlt_setting_entry_t *e = find_entry(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;

    wlt_value_t old = e->value;
    copy_value(&e->value, &e->desc.default_value, e->desc.type);
    e->dirty = true;
    notify(s, key, WLT_EVENT_RESET, &old, &e->value);
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_reset_category(wlt_settings_t *s, wlt_category_t cat)
{
    if (!s) return WLT_SETTINGS_ERR_INVALID;
    if (cat < 0 || cat >= WLT_CAT_COUNT)
        return WLT_SETTINGS_ERR_INVALID;
    for (size_t i = 0; i < s->count; i++) {
        if (s->entries[i].desc.category == cat)
            wlt_settings_reset(s, s->entries[i].desc.key);
    }
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Persistence                                                        */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_save_category(wlt_settings_t *s, wlt_category_t cat)
{
    if (!s || !s->io || !s->io->save_category)
        return WLT_SETTINGS_ERR_INVALID;
    return s->io->save_category(s, cat);
}

wlt_settings_err_t wlt_settings_load_category(wlt_settings_t *s, wlt_category_t cat)
{
    if (!s || !s->io || !s->io->load_category)
        return WLT_SETTINGS_ERR_INVALID;
    return s->io->load_category(s, cat);
}

wlt_settings_err_t wlt_settings_save(wlt_settings_t *s)
{
    if (!s) return WLT_SETTINGS_ERR_INVALID;
    wlt_settings_err_t worst = WLT_SETTINGS_OK;
    for (wlt_category_t c = 0; c < WLT_CAT_COUNT; c++) {
        wlt_settings_err_t r = wlt_settings_save_category(s, c);
        if (r != WLT_SETTINGS_OK && r != WLT_SETTINGS_ERR_NOTFOUND)
            worst = r;
    }
    /* All dirty flags cleared once everything is persisted. */
    if (worst == WLT_SETTINGS_OK) {
        for (size_t i = 0; i < s->count; i++)
            s->entries[i].dirty = false;
    }
    return worst;
}

wlt_settings_err_t wlt_settings_load(wlt_settings_t *s)
{
    if (!s) return WLT_SETTINGS_ERR_INVALID;
    for (wlt_category_t c = 0; c < WLT_CAT_COUNT; c++)
        (void)wlt_settings_load_category(s, c);
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Change notification                                                */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_add_listener(wlt_settings_t *s,
                                             wlt_listener_cb_t cb, void *user)
{
    if (!s || !cb)
        return WLT_SETTINGS_ERR_INVALID;
    if (s->listener_count >= WLT_SETTINGS_MAX_LISTENERS)
        return WLT_SETTINGS_ERR_FULL;
    /* Reject exact duplicates. */
    for (size_t i = 0; i < s->listener_count; i++)
        if (s->listeners[i].cb == cb && s->listeners[i].user == user)
            return WLT_SETTINGS_ERR_INVALID;
    s->listeners[s->listener_count].cb   = cb;
    s->listeners[s->listener_count].user = user;
    s->listener_count++;
    return WLT_SETTINGS_OK;
}

wlt_settings_err_t wlt_settings_remove_listener(wlt_settings_t *s,
                                                wlt_listener_cb_t cb, void *user)
{
    if (!s || !cb)
        return WLT_SETTINGS_ERR_INVALID;
    for (size_t i = 0; i < s->listener_count; i++) {
        if (s->listeners[i].cb == cb && s->listeners[i].user == user) {
            memmove(&s->listeners[i], &s->listeners[i + 1],
                    (s->listener_count - i - 1) * sizeof(s->listeners[0]));
            s->listener_count--;
            return WLT_SETTINGS_OK;
        }
    }
    return WLT_SETTINGS_ERR_NOTFOUND;
}

/* ------------------------------------------------------------------ */
/* Auth                                                               */
/* ------------------------------------------------------------------ */
void wlt_settings_set_auth_cb(wlt_settings_t *s, wlt_auth_cb_t cb, void *user)
{
    if (!s) return;
    s->auth_cb   = cb;
    s->auth_user = user;
}

/* ------------------------------------------------------------------ */
/* Validation (public)                                                */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_validate(wlt_settings_t *s, const char *key,
                                         const wlt_value_t *v,
                                         wlt_setting_type_t type)
{
    const wlt_setting_entry_t *e = find_entry_const(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (!v) return WLT_SETTINGS_ERR_INVALID;
    return validate_against(&e->desc, v, type);
}

wlt_settings_err_t wlt_settings_load_value(wlt_settings_t *s, const char *key,
                                           const wlt_value_t *v,
                                           wlt_setting_type_t type)
{
    wlt_setting_entry_t *e = find_entry(s, key);
    if (!e) return WLT_SETTINGS_ERR_NOTFOUND;
    if (!v) return WLT_SETTINGS_ERR_INVALID;
    if (e->desc.type != type)
        return WLT_SETTINGS_ERR_TYPE;

    /* Loaded values must still validate; a corrupt file is rejected. */
    wlt_settings_err_t vr = validate_against(&e->desc, v, type);
    if (vr != WLT_SETTINGS_OK)
        return vr;

    wlt_value_t old = e->value;
    copy_value(&e->value, v, type);
    e->dirty = false;                          /* now matches disk */
    notify(s, key, WLT_EVENT_LOADED, &old, &e->value);
    return WLT_SETTINGS_OK;
}

/* ------------------------------------------------------------------ */
/* Panels                                                             */
/* ------------------------------------------------------------------ */
wlt_settings_err_t wlt_settings_register_panel(wlt_settings_t *s,
                                               const wlt_panel_t *panel)
{
    if (!s || !panel || !panel->register_settings)
        return WLT_SETTINGS_ERR_INVALID;
    if (s->panel_count >= WLT_SETTINGS_MAX_PANELS)
        return WLT_SETTINGS_ERR_FULL;
    s->panels[s->panel_count++] = panel;
    return panel->register_settings(s);
}

wlt_settings_err_t wlt_settings_apply_category(wlt_settings_t *s, wlt_category_t cat)
{
    if (!s) return WLT_SETTINGS_ERR_INVALID;
    if (cat < 0 || cat >= WLT_CAT_COUNT)
        return WLT_SETTINGS_ERR_INVALID;
    wlt_settings_err_t worst = WLT_SETTINGS_OK;
    for (size_t i = 0; i < s->panel_count; i++) {
        const wlt_panel_t *p = s->panels[i];
        if (p->category == cat && p->apply) {
            wlt_settings_err_t r = p->apply(s);
            if (r != WLT_SETTINGS_OK)
                worst = r;
        }
    }
    return worst;
}

wlt_settings_err_t wlt_settings_apply_all(wlt_settings_t *s)
{
    if (!s) return WLT_SETTINGS_ERR_INVALID;
    wlt_settings_err_t worst = WLT_SETTINGS_OK;
    for (size_t i = 0; i < s->panel_count; i++) {
        if (s->panels[i]->apply) {
            wlt_settings_err_t r = s->panels[i]->apply(s);
            if (r != WLT_SETTINGS_OK)
                worst = r;
        }
    }
    return worst;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
const char *wlt_settings_err_str(wlt_settings_err_t err)
{
    switch (err) {
    case WLT_SETTINGS_OK:              return "ok";
    case WLT_SETTINGS_ERR_INVALID:     return "invalid argument";
    case WLT_SETTINGS_ERR_NOTFOUND:    return "setting not found";
    case WLT_SETTINGS_ERR_TYPE:        return "type mismatch";
    case WLT_SETTINGS_ERR_RANGE:       return "value out of range";
    case WLT_SETTINGS_ERR_PERM:        return "permission denied";
    case WLT_SETTINGS_ERR_IO:          return "I/O error";
    case WLT_SETTINGS_ERR_FULL:        return "capacity reached";
    case WLT_SETTINGS_ERR_NOMEM:       return "out of memory";
    case WLT_SETTINGS_ERR_AUTH:        return "authentication required";
    case WLT_SETTINGS_ERR_UNREACHABLE: return "module unreachable";
    case WLT_SETTINGS_ERR_ENUM:        return "value not in enum";
    default:                           return "unknown error";
    }
}

const char *wlt_settings_category_str(wlt_category_t cat)
{
    switch (cat) {
    case WLT_CAT_NETWORK:  return "network";
    case WLT_CAT_DISPLAY:  return "display";
    case WLT_CAT_SOUND:    return "sound";
    case WLT_CAT_SECURITY: return "security";
    case WLT_CAT_PRIVACY:  return "privacy";
    case WLT_CAT_ACCOUNTS: return "accounts";
    case WLT_CAT_UPDATES:  return "updates";
    case WLT_CAT_APPS:     return "apps";
    case WLT_CAT_SYSTEM:   return "system";
    case WLT_CAT_ABOUT:    return "about";
    default:               return "unknown";
    }
}

const char *wlt_settings_type_str(wlt_setting_type_t type)
{
    switch (type) {
    case WLT_TYPE_BOOL:   return "bool";
    case WLT_TYPE_INT:    return "int";
    case WLT_TYPE_FLOAT:  return "float";
    case WLT_TYPE_STRING: return "string";
    case WLT_TYPE_ENUM:   return "enum";
    default:              return "unknown";
    }
}

/* ------------------------------------------------------------------ */
/* Config I/O indirection                                             */
/* ------------------------------------------------------------------ */
void wlt_settings_set_config_io(wlt_settings_t *s, const wlt_config_io_t *io)
{
    if (!s) return;
    s->io = io ? io : &wlt_default_config_io;
}

/*
 * The default backend (wlt_default_config_io) is defined in config_io.c.
 * It is NOT defined here on purpose: settings.c references it by address,
 * which forces the linker to pull config_io.o from the archive so the real
 * load/save implementation is always present. (A weak fallback would let
 * the linker drop config_io.o entirely, silently leaving NULL backends.)
 */
