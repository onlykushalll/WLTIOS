/**
 * @file permissions.h
 * @brief Public API for WLT-Permissions (app permission manager)
 * @module wlt-permissions
 * @purpose Grants, revokes, checks, and audits app permissions
 * @dependencies: manifest.h
 * @tested_by: tests/userspace/permissions_test.c
 * @spec: docs/specs/wlt-permissions.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_PERMS_H
#define WLT_PERMS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/* âââ Error Codes ââââââââââââââââââââââââââââââââââââââ */

typedef enum {
    WLT_PERMS_OK = 0,
    WLT_PERMS_ERR_INVALID_ARG = -1,
    WLT_PERMS_ERR_NOT_FOUND   = -2,  /* app or grant file missing      */
    WLT_PERMS_ERR_IO          = -3,
    WLT_PERMS_ERR_NOMEM       = -4,
    WLT_PERMS_ERR_PERMISSION  = -5,
    WLT_PERMS_ERR_PARSE       = -6,
    WLT_PERMS_ERR_FULL        = -7,  /* manager at WLT_PERMS_MAX_APPS  */
    WLT_PERMS_ERR_UNDECLARED  = -8,  /* perm not in manifest           */
    WLT_PERMS_ERR_ILLEGAL_VAL = -9,  /* value wrong for perm kind      */
    WLT_PERMS_ERR_TOO_LARGE   = -10, /* path or value too long         */
} wlt_perms_error_t;

/* âââ Constants ââââââââââââââââââââââââââââââââââââââââ */

#define WLT_PERMS_MAX_APPS        64
#define WLT_PERMS_MAX_BY          32    /* actor string ("user"/"system") */
#define WLT_PERMS_MAX_DETAIL      96
#define WLT_PERMS_GRANTS_DIR      "/var/wlt/permissions"
#define WLT_PERMS_LOG_DIR         "/var/log/wlt"
#define WLT_PERMS_AUDIT_LOG       "/var/log/wlt/permissions.log"
#define WLT_PERMS_GRANT_FILE_MAX  4096
/* A grant path is <grants_dir>(â¤255) + '/' + <app>(â¤63) + ".ini" + NUL,
 * so 336 is provably large enough for snprintf without truncation. */
#define WLT_PERMS_GRANT_PATH_MAX  (WLT_PERMS_MAX_PATH + WLT_PERMS_MAX_NAME + 16)

/* âââ Decision (derived from a value) ââââââââââââââââââ
 * The flat value enum distinguishes tor-only vs direct, but a caller
 * asking "may the app do X?" only wants allow/ask/deny. */
typedef enum {
    WLT_PERMS_DECISION_DENY  = 0,
    WLT_PERMS_DECISION_ASK   = 1,
    WLT_PERMS_DECISION_ALLOW = 2,
} wlt_perms_decision_t;

/* âââ Privacy Badge ââââââââââââââââââââââââââââââââââââ */

typedef enum {
    WLT_PERMS_BADGE_GREEN  = 0,   /* privacy-respecting             */
    WLT_PERMS_BADGE_YELLOW = 1,   /* review recommended             */
    WLT_PERMS_BADGE_RED    = 2,   /* high risk                      */
} wlt_perms_badge_t;

/* âââ Audit Level ââââââââââââââââââââââââââââââââââââââ */

typedef enum {
    WLT_PERMS_AUDIT_INFO     = 0,
    WLT_PERMS_AUDIT_WARN     = 1,
    WLT_PERMS_AUDIT_GRANTED  = 2,
    WLT_PERMS_AUDIT_REVOKED  = 3,
    WLT_PERMS_AUDIT_DENIED   = 4,
    WLT_PERMS_AUDIT_SECURITY = 5,  /* undeclared permission requested */
} wlt_perms_audit_level_t;

/* âââ Per-App Record âââââââââââââââââââââââââââââââââââ
 * grant[] is the effective (user-approved) state; manifest.values[]
 * is what the app requested. They diverge once the user revokes or
 * re-grants a permission. declared_mask is cached from the manifest
 * so check() can reject undeclared requests without re-parsing. */
typedef struct {
    char id[WLT_PERMS_MAX_NAME];
    wlt_PermsManifest manifest;
    uint32_t declared_mask;
    wlt_perms_value_t grant[WLT_PERM_COUNT];
    uint64_t updated_at;                 /* epoch seconds of last change */
    char updated_by[WLT_PERMS_MAX_BY];
    bool registered;
} wlt_PermsApp;

typedef struct {
    wlt_PermsApp apps[WLT_PERMS_MAX_APPS];
    size_t app_count;
    char grants_dir[WLT_PERMS_MAX_PATH];
    bool initialised;
} wlt_PermsManager;

/* One row of wlt_perms_list(): requested vs granted side by side. */
typedef struct {
    wlt_perms_id_t perm_id;
    bool declared;
    wlt_perms_value_t requested;         /* manifest value           */
    wlt_perms_value_t granted;           /* effective value          */
    wlt_perms_decision_t decision;       /* derived from granted     */
} wlt_PermsEntry;

/* âââ Lifecycle ââââââââââââââââââââââââââââââââââââââââ */

wlt_perms_error_t wlt_perms_init(wlt_PermsManager *m, const char *grants_dir);
void wlt_perms_shutdown(wlt_PermsManager *m);

/* âââ Registration âââââââââââââââââââââââââââââââââââââ
 * Registering an app seeds grant[] from the manifest: a perm declared
 * "allow" is granted at install time, "ask" stays pending (ASK), and
 * "deny"/undeclared stays DENY. This mirrors Android's split between
 * install-time and runtime permissions while keeping undeclared caps
 * denied by default. */

wlt_perms_error_t wlt_perms_register(wlt_PermsManager *m,
                                     const wlt_PermsManifest *manifest);

wlt_PermsApp *wlt_perms_find(wlt_PermsManager *m, const char *app_id);
size_t wlt_perms_app_count(const wlt_PermsManager *m);

/* âââ Check / Get ââââââââââââââââââââââââââââââââââââââ */

wlt_perms_decision_t wlt_perms_check(const wlt_PermsManager *m,
                                     const char *app_id,
                                     wlt_perms_id_t perm);

wlt_perms_error_t wlt_perms_get(const wlt_PermsManager *m,
                                const char *app_id,
                                wlt_perms_id_t perm,
                                wlt_perms_value_t *out);

/* Map an effective value to its decision. Exposed for badges.c. */
wlt_perms_decision_t wlt_perms_decision_of(wlt_perms_id_t id,
                                           wlt_perms_value_t v);

/* âââ Grant / Revoke âââââââââââââââââââââââââââââââââââ
 * Both record an audit line and update updated_at/by. grant() rejects
 * values illegal for the perm kind (e.g. FS_FULL for camera) and
 * values for undeclared permissions â the spec requires that an app
 * cannot be granted a cap it never declared. */

wlt_perms_error_t wlt_perms_grant(wlt_PermsManager *m,
                                  const char *app_id,
                                  wlt_perms_id_t perm,
                                  wlt_perms_value_t value,
                                  const char *by);

wlt_perms_error_t wlt_perms_revoke(wlt_PermsManager *m,
                                   const char *app_id,
                                   wlt_perms_id_t perm,
                                   const char *by);

/* âââ Runtime Request ââââââââââââââââââââââââââââââââââ
 * Called when an app requests a capability at runtime. Returns the
 * decision the shell should act on: ALLOW (already/now granted),
 * ASK (declared "ask" and not yet decided â show dialog), or DENY
 * (declared "deny", revoked, or undeclared). Undeclared requests are
 * logged as WLT_PERMS_AUDIT_SECURITY per spec Â§Error. */
wlt_perms_error_t wlt_perms_request(wlt_PermsManager *m,
                                    const char *app_id,
                                    wlt_perms_id_t perm,
                                    wlt_perms_decision_t *out_decision,
                                    const char *by);

/* âââ List âââââââââââââââââââââââââââââââââââââââââââââ
 * Fill @entries with one row per permission. On entry *@count holds
 * the array capacity; on return it holds the number written. */
wlt_perms_error_t wlt_perms_list(const wlt_PermsManager *m,
                                 const char *app_id,
                                 wlt_PermsEntry *entries,
                                 size_t *count);

/* âââ Persistence ââââââââââââââââââââââââââââââââââââââ
 * Grant state lives in <grants_dir>/<app>.ini so it survives reboot.
 * load() overlays grant[] onto an already-registered app; save()
 * writes the current grant[]. */

wlt_perms_error_t wlt_perms_save(const wlt_PermsManager *m,
                                 const char *app_id);

wlt_perms_error_t wlt_perms_load(wlt_PermsManager *m,
                                 const char *app_id);

/**
 * Rebuild the manager from every *.manifest in @apps_dir: load and
 * register each, then overlay saved grants. Used to recover from a
 * corrupt grant database (spec Â§Error: "Rebuild from manifests").
 */
wlt_perms_error_t wlt_perms_rebuild(wlt_PermsManager *m,
                                    const char *apps_dir);

/* âââ Badges (badges.c) ââââââââââââââââââââââââââââââââ */

wlt_perms_badge_t wlt_perms_badge(const wlt_PermsManager *m,
                                  const char *app_id);
wlt_perms_badge_t wlt_perms_badge_for_app(const wlt_PermsApp *app);
const char *wlt_perms_badge_str(wlt_perms_badge_t b);
const char *wlt_perms_badge_emoji(wlt_perms_badge_t b);

/* âââ Audit (audit.c) ââââââââââââââââââââââââââââââââââ
 * Append-only log at WLT_PERMS_AUDIT_LOG. Lines look like:
 *   2026-06-22T13:01:07Z GRANTED  app=firefox perm=camera by=user ...
 * The fd is opened O_APPEND|O_CLOEXEC so the launcher forked by
 * wlt-sandbox does not inherit it, and concurrent writers stay atomic
 * up to PIPE_BUF. */

wlt_perms_error_t wlt_perms_audit_init(void);
void wlt_perms_audit_close(void);
void wlt_perms_audit_log(wlt_perms_audit_level_t level,
                         const char *app_id,
                         wlt_perms_id_t perm,
                         const char *action,
                         const char *by,
                         const char *detail);

/* âââ String Helpers âââââââââââââââââââââââââââââââââââ */

const char *wlt_perms_error_str(wlt_perms_error_t err);
const char *wlt_perms_decision_str(wlt_perms_decision_t d);
const char *wlt_perms_audit_level_str(wlt_perms_audit_level_t l);

#ifdef __cplusplus
}
#endif

#endif /* WLT_PERMS_H */
