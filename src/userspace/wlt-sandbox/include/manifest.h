/**
 * @file manifest.h
 * @brief App manifest parser for WLT-Sandbox
 * @module wlt-sandbox/manifest
 * @purpose Parses INI-style .manifest files from /etc/wlt/apps/
 * @dependencies: none
 * @tested_by: tests/userspace/manifest_test.c
 * @spec: docs/specs/wlt-sandbox.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_MANIFEST_H
#define WLT_MANIFEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_MANIFEST_OK = 0,
    WLT_MANIFEST_ERR_INVALID_ARG = -1,
    WLT_MANIFEST_ERR_NOT_FOUND   = -2,
    WLT_MANIFEST_ERR_IO          = -3,
    WLT_MANIFEST_ERR_PARSE       = -4,
    WLT_MANIFEST_ERR_NOMEM       = -5,
    WLT_MANIFEST_ERR_TOO_LARGE   = -6,
} wlt_manifest_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_MANIFEST_VERSION          "1.0.0"
#define WLT_MANIFEST_MAX_NAME         64
#define WLT_MANIFEST_MAX_PATH         256
#define WLT_MANIFEST_MAX_ARGS         512
#define WLT_MANIFEST_MAX_LINE         512
#define WLT_MANIFEST_MAX_SECCOMP_GROUPS 8
#define WLT_MANIFEST_MAX_GROUP_NAME   32

/* Default resource limits if the manifest omits them.
 * Chosen to be generous enough for typical GUI apps while still
 * preventing a runaway process from exhausting the system. */
#define WLT_MANIFEST_DEFAULT_MEM_MB   256
#define WLT_MANIFEST_DEFAULT_CPU_PCT  25
#define WLT_MANIFEST_DEFAULT_DISK_MB  512
#define WLT_MANIFEST_DEFAULT_PIDS     128

/* Hard caps — manifest values above these are rejected so a
 * compromised manifest cannot grant itself unlimited resources. */
#define WLT_MANIFEST_MAX_MEM_MB       8192
#define WLT_MANIFEST_MAX_CPU_PCT      100
#define WLT_MANIFEST_MAX_DISK_MB      32768
#define WLT_MANIFEST_MAX_PIDS         4096

/* ─── Types ──────────────────────────────────────────── */

/* Network policy. tor-only forces all traffic through the Tor SOCKS
 * proxy (handled at the net-namespace layer by blocking direct routes). */
typedef enum {
    WLT_NET_TOR_ONLY = 0,
    WLT_NET_DIRECT   = 1,
    WLT_NET_NONE     = 2,
} wlt_manifest_net_mode_t;

/* Filesystem exposure. NONE hides /users entirely; FULL mounts the root
 * (used only by trusted system apps). */
typedef enum {
    WLT_FS_NONE    = 0,
    WLT_FS_HOME_RO = 1,
    WLT_FS_HOME_RW = 2,
    WLT_FS_FULL    = 3,
} wlt_manifest_fs_mode_t;

/* Tri-state permission for capabilities that may prompt the user. */
typedef enum {
    WLT_PERM_DENY  = 0,
    WLT_PERM_ASK   = 1,
    WLT_PERM_ALLOW = 2,
} wlt_manifest_perm_t;

/* Seccomp syscall group IDs. These names appear in the manifest's
 * [seccomp] allowed = ... list. The launcher maps them to BPF rules. */
typedef enum {
    WLT_SECCOMP_GRP_BASIC      = 0,  /* read/write/open/close/mmap/exit */
    WLT_SECCOMP_GRP_NETWORK    = 1,  /* socket/connect/send/recv        */
    WLT_SECCOMP_GRP_FILESYSTEM = 2,  /* stat/readdir/rename/unlink      */
    WLT_SECCOMP_GRP_PROCESS    = 3,  /* fork/clone/wait4                */
    WLT_SECCOMP_GRP_SIGNAL     = 4,  /* sigaction/kill                  */
    WLT_SECCOMP_GRP_IPC        = 5,  /* pipe/shmget                     */
    WLT_SECCOMP_GRP_COUNT            /* sentinel                        */
} wlt_manifest_seccomp_group_t;

/* Resource limits. memory_mb and disk_mb are in mebibytes, cpu_percent
 * is 0..100, pids is a count. A zero value means "use the default". */
typedef struct {
    uint32_t memory_mb;
    uint32_t cpu_percent;
    uint32_t disk_mb;
    uint32_t pids;
    uint32_t network_kbps;   /* 0 = unlimited */
} wlt_SandboxResources;

/* Full parsed manifest. Fields default to safe values (deny/none)
 * when the corresponding key is absent, following the spec's
 * "default deny" rule. */
typedef struct {
    /* [app] */
    char name[WLT_MANIFEST_MAX_NAME];
    char exec[WLT_MANIFEST_MAX_PATH];
    char icon[WLT_MANIFEST_MAX_PATH];
    char args[WLT_MANIFEST_MAX_ARGS];
    char user[32];                  /* Drop to this user before exec */

    /* [permissions] */
    wlt_manifest_net_mode_t  network;
    wlt_manifest_fs_mode_t   filesystem;
    wlt_manifest_perm_t      camera;
    wlt_manifest_perm_t      microphone;
    wlt_manifest_perm_t      location;
    wlt_manifest_perm_t      notifications;
    wlt_manifest_perm_t      clipboard;

    /* [resources] */
    wlt_SandboxResources resources;

    /* [seccomp] — bitmask of WLT_SECCOMP_GRP_* flags */
    uint32_t seccomp_groups;
} wlt_SandboxManifest;

/* ─── Public API ─────────────────────────────────────── */

/**
 * Initialise a manifest struct to safe defaults (deny-everything).
 * Must be called before wlt_manifest_load() populates it.
 */
void wlt_manifest_default(wlt_SandboxManifest *m);

/**
 * Load and parse a .manifest file from disk.
 * @param path  Path to the manifest (e.g. /etc/wlt/apps/firefox.manifest)
 * @param m     Output manifest (call wlt_manifest_default first)
 * @return WLT_MANIFEST_OK on success, error code otherwise
 */
wlt_manifest_error_t wlt_manifest_load(const char *path,
                                       wlt_SandboxManifest *m);

/**
 * Validate a parsed manifest (check exec exists, limits are sane, etc.).
 * Called automatically by wlt_manifest_load() but exposed for tests.
 */
wlt_manifest_error_t wlt_manifest_validate(const wlt_SandboxManifest *m);

/**
 * Parse a comma-separated seccomp group list ("basic, network, ...")
 * into the bitmask field of the manifest.
 */
wlt_manifest_error_t wlt_manifest_parse_seccomp_groups(const char *list,
                                                       uint32_t *out_mask);

/**
 * Convert a manifest enum to a human-readable string.
 */
const char *wlt_manifest_net_str(wlt_manifest_net_mode_t m);
const char *wlt_manifest_fs_str(wlt_manifest_fs_mode_t m);
const char *wlt_manifest_perm_str(wlt_manifest_perm_t p);

#ifdef __cplusplus
}
#endif

#endif /* WLT_MANIFEST_H */
