/**
 * @file permissions.c
 * @brief Permission manager core for WLT-Permissions
 * @module wlt-permissions
 * @purpose Registers apps, grants/revokes/checks permissions, lists state
 * @dependencies: manifest.h, permissions.h
 * @tested_by: tests/userspace/permissions_test.c
 * @spec: docs/specs/wlt-permissions.md
 *
 * Persistence (save/load) and rebuild-from-manifests live in persist.c;
 * audit logging in audit.c; badge calculation in badges.c.
 */

#include "permissions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define WLT_PM_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-permissions][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_PM_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-permissions][WRN] " fmt "\n", ##__VA_ARGS__)

static void
copy_bounded(char *dst, size_t dst_max, const char *src)
{
    if (dst_max == 0)
        return;
    size_t n = strlen(src);
    if (n >= dst_max)
        n = dst_max - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static uint64_t
now_epoch(void)
{
    return (uint64_t)time(NULL);
}

static const wlt_PermsApp *
find_app(const wlt_PermsManager *m, const char *app_id)
{
    if (m == NULL || app_id == NULL)
        return NULL;
    for (size_t i = 0; i < m->app_count; i++)
    {
        if (strcmp(m->apps[i].id, app_id) == 0)
            return &m->apps[i];
    }
    return NULL;
}

static void
touch(wlt_PermsApp *app, const char *by)
{
    app->updated_at = now_epoch();
    copy_bounded(app->updated_by, sizeof(app->updated_by),
                 by ? by : "system");
}

/* âââ Lifecycle ââââââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_init(wlt_PermsManager *m, const char *grants_dir)
{
    if (m == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;
    memset(m, 0, sizeof(*m));
    copy_bounded(m->grants_dir, sizeof(m->grants_dir),
                 grants_dir ? grants_dir : WLT_PERMS_GRANTS_DIR);
    /* Create the grants dir up front so persist.c never has to. */
    if (mkdir(m->grants_dir, 0750) != 0 && errno != EEXIST)
    {
        WLT_PM_LOG_ERR("mkdir(%s): %s", m->grants_dir, strerror(errno));
        return WLT_PERMS_ERR_IO;
    }
    m->initialised = true;
    return WLT_PERMS_OK;
}

void
wlt_perms_shutdown(wlt_PermsManager *m)
{
    if (m == NULL)
        return;
    m->app_count = 0;
    m->initialised = false;
}

/* âââ Registration âââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_register(wlt_PermsManager *m, const wlt_PermsManifest *manifest)
{
    if (m == NULL || manifest == NULL || manifest->name[0] == '\0')
        return WLT_PERMS_ERR_INVALID_ARG;
    if (find_app(m, manifest->name) != NULL)
        return WLT_PERMS_OK;  /* idempotent re-register */
    if (m->app_count >= WLT_PERMS_MAX_APPS)
        return WLT_PERMS_ERR_FULL;

    wlt_PermsApp *app = &m->apps[m->app_count++];
    memset(app, 0, sizeof(*app));
    copy_bounded(app->id, sizeof(app->id), manifest->name);
    app->manifest = *manifest;
    app->declared_mask = manifest->declared_mask;
    /* Seed effective grants from declared requests: "allow" granted at
     * install time, "ask" pending, "deny"/undeclared denied. */
    for (int i = 0; i < WLT_PERM_COUNT; i++)
        app->grant[i] = manifest->values[i];
    app->updated_at = now_epoch();
    copy_bounded(app->updated_by, sizeof(app->updated_by), "install");
    app->registered = true;
    return WLT_PERMS_OK;
}

wlt_PermsApp *
wlt_perms_find(wlt_PermsManager *m, const char *app_id)
{
    return (wlt_PermsApp *)find_app(m, app_id);
}

size_t
wlt_perms_app_count(const wlt_PermsManager *m)
{
    return m ? m->app_count : 0;
}

/* âââ Decision Logic âââââââââââââââââââââââââââââââââââ */

wlt_perms_decision_t
wlt_perms_decision_of(wlt_perms_id_t id, wlt_perms_value_t v)
{
    switch (wlt_perms_id_kind(id))
    {
        case WLT_PERMS_KIND_NETWORK:
            return (v == WLT_PERMS_NET_TOR_ONLY || v == WLT_PERMS_NET_DIRECT)
                       ? WLT_PERMS_DECISION_ALLOW : WLT_PERMS_DECISION_DENY;
        case WLT_PERMS_KIND_FILESYSTEM:
            return (v == WLT_PERMS_FS_HOME_RO || v == WLT_PERMS_FS_HOME_RW ||
                    v == WLT_PERMS_FS_FULL)
                       ? WLT_PERMS_DECISION_ALLOW : WLT_PERMS_DECISION_DENY;
        case WLT_PERMS_KIND_TRI:
            if (v == WLT_PERMS_ALLOW) return WLT_PERMS_DECISION_ALLOW;
            if (v == WLT_PERMS_ASK)   return WLT_PERMS_DECISION_ASK;
            return WLT_PERMS_DECISION_DENY;
        case WLT_PERMS_KIND_BOOL:
            return (v == WLT_PERMS_ALLOW) ? WLT_PERMS_DECISION_ALLOW
                                          : WLT_PERMS_DECISION_DENY;
        default:
            return WLT_PERMS_DECISION_DENY;
    }
}

wlt_perms_decision_t
wlt_perms_check(const wlt_PermsManager *m, const char *app_id,
                wlt_perms_id_t perm)
{
    const wlt_PermsApp *app = find_app(m, app_id);
    if (app == NULL || (app->declared_mask & (1u << perm)) == 0)
        return WLT_PERMS_DECISION_DENY;  /* missing or undeclared */
    return wlt_perms_decision_of(perm, app->grant[perm]);
}

wlt_perms_error_t
wlt_perms_get(const wlt_PermsManager *m, const char *app_id,
              wlt_perms_id_t perm, wlt_perms_value_t *out)
{
    if (out == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;
    const wlt_PermsApp *app = find_app(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;
    *out = app->grant[perm];
    return WLT_PERMS_OK;
}

/* âââ Grant / Revoke âââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_grant(wlt_PermsManager *m, const char *app_id,
                wlt_perms_id_t perm, wlt_perms_value_t value,
                const char *by)
{
    if (perm < 0 || perm >= WLT_PERM_COUNT)
        return WLT_PERMS_ERR_INVALID_ARG;
    wlt_PermsApp *app = wlt_perms_find(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;
    /* An app cannot be granted a cap it never declared. */
    if ((app->declared_mask & (1u << perm)) == 0)
    {
        wlt_perms_audit_log(WLT_PERMS_AUDIT_SECURITY, app_id, perm,
                            "grant-undeclared", by,
                            "permission not declared in manifest");
        return WLT_PERMS_ERR_UNDECLARED;
    }
    if (!wlt_perms_value_is_legal(perm, value))
        return WLT_PERMS_ERR_ILLEGAL_VAL;

    app->grant[perm] = value;
    touch(app, by);
    wlt_perms_audit_log(WLT_PERMS_AUDIT_GRANTED, app_id, perm, "grant", by,
                        wlt_perms_value_str(perm, value));
    wlt_perms_error_t se = wlt_perms_save(m, app_id);  /* best-effort */
    if (se != WLT_PERMS_OK)
        WLT_PM_LOG_WRN("save(%s) failed: %s", app_id, wlt_perms_error_str(se));
    return WLT_PERMS_OK;
}

wlt_perms_error_t
wlt_perms_revoke(wlt_PermsManager *m, const char *app_id,
                 wlt_perms_id_t perm, const char *by)
{
    if (perm < 0 || perm >= WLT_PERM_COUNT)
        return WLT_PERMS_ERR_INVALID_ARG;
    wlt_PermsApp *app = wlt_perms_find(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;

    app->grant[perm] = WLT_PERMS_DENY;
    touch(app, by);
    wlt_perms_audit_log(WLT_PERMS_AUDIT_REVOKED, app_id, perm, "revoke", by,
                        wlt_perms_id_str(perm));
    wlt_perms_error_t se = wlt_perms_save(m, app_id);  /* best-effort */
    if (se != WLT_PERMS_OK)
        WLT_PM_LOG_WRN("save(%s) failed: %s", app_id, wlt_perms_error_str(se));
    return WLT_PERMS_OK;
}

/* âââ Runtime Request ââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_request(wlt_PermsManager *m, const char *app_id,
                  wlt_perms_id_t perm, wlt_perms_decision_t *out_decision,
                  const char *by)
{
    if (out_decision == NULL || perm < 0 || perm >= WLT_PERM_COUNT)
        return WLT_PERMS_ERR_INVALID_ARG;

    *out_decision = WLT_PERMS_DECISION_DENY;
    const wlt_PermsApp *app = find_app(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;

    /* Undeclared request: deny and flag a security event (spec Â§Error). */
    if ((app->declared_mask & (1u << perm)) == 0)
    {
        wlt_perms_audit_log(WLT_PERMS_AUDIT_SECURITY, app_id, perm,
                            "request-undeclared", by,
                            "permission not declared in manifest");
        return WLT_PERMS_OK;  /* processed; the answer is DENY */
    }

    wlt_perms_decision_t d = wlt_perms_decision_of(perm, app->grant[perm]);
    *out_decision = d;
    wlt_perms_audit_log(WLT_PERMS_AUDIT_INFO, app_id, perm, "request", by,
                        wlt_perms_decision_str(d));
    return WLT_PERMS_OK;
}

/* âââ List âââââââââââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t
wlt_perms_list(const wlt_PermsManager *m, const char *app_id,
               wlt_PermsEntry *entries, size_t *count)
{
    if (entries == NULL || count == NULL)
        return WLT_PERMS_ERR_INVALID_ARG;
    const wlt_PermsApp *app = find_app(m, app_id);
    if (app == NULL)
        return WLT_PERMS_ERR_NOT_FOUND;

    size_t cap = *count;
    size_t n = 0;
    for (int i = 0; i < WLT_PERM_COUNT && n < cap; i++)
    {
        wlt_PermsEntry *e = &entries[n++];
        e->perm_id = (wlt_perms_id_t)i;
        e->declared = (app->declared_mask & (1u << i)) != 0;
        e->requested = app->manifest.values[i];
        e->granted = app->grant[i];
        e->decision = wlt_perms_decision_of((wlt_perms_id_t)i, app->grant[i]);
    }
    *count = n;
    return WLT_PERMS_OK;
}

/* âââ String Helpers âââââââââââââââââââââââââââââââââââ */

const char *
wlt_perms_error_str(wlt_perms_error_t err)
{
    switch (err)
    {
        case WLT_PERMS_OK:              return "ok";
        case WLT_PERMS_ERR_INVALID_ARG: return "invalid argument";
        case WLT_PERMS_ERR_NOT_FOUND:   return "not found";
        case WLT_PERMS_ERR_IO:          return "i/o error";
        case WLT_PERMS_ERR_NOMEM:       return "out of memory";
        case WLT_PERMS_ERR_PERMISSION:  return "permission denied";
        case WLT_PERMS_ERR_PARSE:       return "parse error";
        case WLT_PERMS_ERR_FULL:        return "app table full";
        case WLT_PERMS_ERR_UNDECLARED:  return "permission not declared";
        case WLT_PERMS_ERR_ILLEGAL_VAL: return "illegal value for permission";
        case WLT_PERMS_ERR_TOO_LARGE:   return "too large";
        default:                        return "unknown error";
    }
}

const char *
wlt_perms_decision_str(wlt_perms_decision_t d)
{
    switch (d)
    {
        case WLT_PERMS_DECISION_DENY:  return "deny";
        case WLT_PERMS_DECISION_ASK:   return "ask";
        case WLT_PERMS_DECISION_ALLOW: return "allow";
        default:                       return "?";
    }
}

const char *
wlt_perms_audit_level_str(wlt_perms_audit_level_t l)
{
    switch (l)
    {
        case WLT_PERMS_AUDIT_INFO:     return "INFO";
        case WLT_PERMS_AUDIT_WARN:     return "WARN";
        case WLT_PERMS_AUDIT_GRANTED:  return "GRANTED";
        case WLT_PERMS_AUDIT_REVOKED:  return "REVOKED";
        case WLT_PERMS_AUDIT_DENIED:   return "DENIED";
        case WLT_PERMS_AUDIT_SECURITY: return "SECURITY";
        default:                       return "UNKNOWN";
    }
}
