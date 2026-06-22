/**
 * @file manifest.h
 * @brief App manifest types and parser for WLT-Permissions
 * @module wlt-permissions/manifest
 * @purpose Parses INI-style .manifest files declaring app permissions
 * @dependencies: none
 * @tested_by: tests/userspace/permissions_manifest_test.c
 * @spec: docs/specs/wlt-permissions.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_PERMS_MANIFEST_H
#define WLT_PERMS_MANIFEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* âââ Error Codes ââââââââââââââââââââââââââââââââââââââ */

typedef enum {
    WLT_PERMS_MFST_OK = 0,
    WLT_PERMS_MFST_ERR_INVALID_ARG = -1,
    WLT_PERMS_MFST_ERR_NOT_FOUND   = -2,
    WLT_PERMS_MFST_ERR_IO          = -3,
    WLT_PERMS_MFST_ERR_PARSE       = -4,
    WLT_PERMS_MFST_ERR_NOMEM       = -5,
    WLT_PERMS_MFST_ERR_TOO_LARGE   = -6,
} wlt_perms_manifest_error_t;

/* âââ Constants ââââââââââââââââââââââââââââââââââââââââ */

#define WLT_PERMS_VERSION        "1.0.0"
#define WLT_PERMS_MAX_NAME       64
#define WLT_PERMS_MAX_PATH       256
#define WLT_PERMS_MAX_LINE       512
#define WLT_PERMS_APP_DIR        "/etc/wlt/apps"
#define WLT_PERMS_MANIFEST_SUFFIX ".manifest"

/* âââ Permission Identifiers âââââââââââââââââââââââââââ
 * One id per capability. The tri-state range (CAMERA..USB) and the
 * boolean range (BACKGROUND..AUTOSTART) are contiguous so a perm can
 * be classified by id alone â see wlt_perms_id_kind(). Order must
 * match the lookup table in manifest.c. */
typedef enum {
    WLT_PERM_NETWORK       = 0,
    WLT_PERM_FILESYSTEM    = 1,
    WLT_PERM_CAMERA        = 2,   /* tri-state range starts here */
    WLT_PERM_MICROPHONE    = 3,
    WLT_PERM_LOCATION      = 4,
    WLT_PERM_NOTIFICATIONS = 5,
    WLT_PERM_CLIPBOARD     = 6,
    WLT_PERM_CONTACTS      = 7,
    WLT_PERM_BLUETOOTH     = 8,
    WLT_PERM_USB           = 9,   /* tri-state range ends here   */
    WLT_PERM_BACKGROUND    = 10,  /* boolean range starts here   */
    WLT_PERM_AUTOSTART     = 11,  /* boolean range ends here     */
    WLT_PERM_COUNT         = 12,
} wlt_perms_id_t;

/* Classification of a permission id. Drives value validation: a
 * network perm only accepts network modes, a tri perm only accepts
 * deny/ask/allow, etc. */
typedef enum {
    WLT_PERMS_KIND_NETWORK    = 0,
    WLT_PERMS_KIND_FILESYSTEM = 1,
    WLT_PERMS_KIND_TRI        = 2,
    WLT_PERMS_KIND_BOOL       = 3,
} wlt_perms_kind_t;

/* âââ Permission Values ââââââââââââââââââââââââââââââââ
 * A single enum spans every value any permission may hold. DENY is
 * overloaded to mean "none" for network and filesystem (the spec
 * treats "none" as the denied state for those capabilities), which
 * keeps the grant store a flat wlt_perms_value_t[12]. */
typedef enum {
    WLT_PERMS_DENY         = 0,   /* no access (== network none / fs none) */
    WLT_PERMS_ASK          = 1,   /* prompt user at runtime                */
    WLT_PERMS_ALLOW        = 2,   /* access granted                        */
    /* network modes â valid only for WLT_PERM_NETWORK */
    WLT_PERMS_NET_TOR_ONLY = 10,
    WLT_PERMS_NET_DIRECT   = 11,
    /* filesystem modes â valid only for WLT_PERM_FILESYSTEM */
    WLT_PERMS_FS_HOME_RO   = 20,
    WLT_PERMS_FS_HOME_RW   = 21,
    WLT_PERMS_FS_FULL      = 22,
} wlt_perms_value_t;

/* âââ Manifest Type ââââââââââââââââââââââââââââââââââââ
 * Parsed form of a .manifest file. values[] holds the declared value
 * for each permission; a perm is "declared" iff its bit in
 * declared_mask is set. Unmentioned perms default to DENY and are
 * undeclared â requesting one is a security event (spec Â§Error). */
typedef struct {
    /* [app] */
    char name[WLT_PERMS_MAX_NAME];
    char exec[WLT_PERMS_MAX_PATH];
    char icon[WLT_PERMS_MAX_PATH];

    /* [permissions] */
    wlt_perms_value_t values[WLT_PERM_COUNT];
    uint32_t declared_mask;
    bool has_permissions_section;
} wlt_PermsManifest;

/* âââ Public API âââââââââââââââââââââââââââââââââââââââ */

/**
 * Reset a manifest to safe defaults (deny-everything, nothing declared).
 * Must be called before wlt_perms_manifest_load() populates it.
 */
void wlt_perms_manifest_default(wlt_PermsManifest *m);

/**
 * Load and parse a .manifest file from disk.
 * @param path  e.g. /etc/wlt/apps/firefox.manifest
 * @param m     output manifest (call wlt_perms_manifest_default first)
 */
wlt_perms_manifest_error_t wlt_perms_manifest_load(const char *path,
                                                   wlt_PermsManifest *m);

/**
 * Parse manifest text already in memory. Exposed so tests can feed
 * fixtures without touching disk.
 */
wlt_perms_manifest_error_t wlt_perms_manifest_parse_text(const char *text,
                                                         wlt_PermsManifest *m);

/**
 * Validate a parsed manifest: name non-empty, exec non-empty, every
 * declared value legal for its permission kind. Called automatically
 * by wlt_perms_manifest_load() but exposed for tests.
 */
wlt_perms_manifest_error_t wlt_perms_manifest_validate(const wlt_PermsManifest *m);

/* âââ Value / Id Helpers âââââââââââââââââââââââââââââââ */

wlt_perms_kind_t wlt_perms_id_kind(wlt_perms_id_t id);
const char *wlt_perms_id_str(wlt_perms_id_t id);

/** Human-readable value in the context of @id. Network/filesystem
 * render their denied state as "none" (matching the manifest grammar);
 * tri/bool render it as "deny". */
const char *wlt_perms_value_str(wlt_perms_id_t id, wlt_perms_value_t v);

/** Map a manifest key ("network", "camera", ...) to a perm id.
 * Returns WLT_PERM_COUNT if the key is not a known permission. */
wlt_perms_id_t wlt_perms_id_from_str(const char *key);

/** Parse a value string in the context of a permission kind.
 * Accepts the generic "deny"/"ask"/"allow" for every kind plus the
 * kind-specific tokens ("tor-only", "home:rw", ...). */
wlt_perms_manifest_error_t wlt_perms_value_from_str(wlt_perms_kind_t kind,
                                                    const char *str,
                                                    wlt_perms_value_t *out);

/** True if @v is a legal effective value for permission @id. */
bool wlt_perms_value_is_legal(wlt_perms_id_t id, wlt_perms_value_t v);

const char *wlt_perms_manifest_error_str(wlt_perms_manifest_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_PERMS_MANIFEST_H */
