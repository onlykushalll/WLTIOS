/**
 * @file update.h
 * @brief Public API for WLT-Update (atomic A/B partition update system)
 * @module wlt-update
 * @purpose Checks the repository over Tor, downloads signed update
 *          packages, verifies their Ed25519 signature, and installs them
 *          to the inactive partition so a reboot atomically switches OS
 *          versions â with automatic rollback on boot failure.
 * @dependencies: partition.h, libsodium (Ed25519), POSIX sockets
 * @tested_by: tests/userspace/wlt_update_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_UPDATE_H
#define WLT_UPDATE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ââ Error codes (0 = success, negative = error) ââ */
typedef enum {
    WLT_UPDATE_OK = 0,
    WLT_UPDATE_ERR_INVALID_ARG = -1,
    WLT_UPDATE_ERR_NOT_FOUND = -2,
    WLT_UPDATE_ERR_PERMISSION = -3,
    WLT_UPDATE_ERR_IO = -4,
    WLT_UPDATE_ERR_NOMEM = -5,
    WLT_UPDATE_ERR_NETWORK = -6,        /* generic transport failure      */
    WLT_UPDATE_ERR_TOR = -7,            /* Tor SOCKS unreachable           */
    WLT_UPDATE_ERR_SIGNATURE = -8,      /* Ed25519 verify failed â REFUSE  */
    WLT_UPDATE_ERR_DISK_FULL = -9,
    WLT_UPDATE_ERR_PARTITION = -10,
    WLT_UPDATE_ERR_BOOT_CONFIG = -11,
    WLT_UPDATE_ERR_DOWNGRADE = -12,     /* target version not newer        */
    WLT_UPDATE_ERR_UNSUPPORTED = -13,   /* built without libsodium         */
    WLT_UPDATE_ERR_TIMEOUT = -14,
    WLT_UPDATE_ERR_ALREADY_UP_TO_DATE = -15,
    WLT_UPDATE_ERR_PENDING_REBOOT = -16,
    WLT_UPDATE_ERR_INSUFFICIENT_SPACE = -17,  /* pre-format free-space check */
    WLT_UPDATE_ERR_BUSY = -18,                /* another wltupdate holds the lock */
} wlt_update_error_t;

/* ââ Constants ââ */
#define WLT_UPDATE_VERSION            "1.0.0"
#define WLT_UPDATE_CONFIG_PATH        "/etc/wlt/update.conf"
#define WLT_UPDATE_BOOT_CONFIG_PATH   "/boot/wltios.conf"
#define WLT_UPDATE_STATUS_PATH        "/var/lib/wlt/update.status"
#define WLT_UPDATE_ROLLBACK_PATH      "/var/lib/wlt/rollback.counter"
#define WLT_UPDATE_HISTORY_PATH       "/var/log/wlt/update.history"
#define WLT_UPDATE_DOWNLOAD_DIR       "/var/cache/wlt-update"
#define WLT_UPDATE_PUBKEY_PATH        "/etc/wlt/keys/update-pubkey.bin"
#define WLT_UPDATE_ACTIVE_MARKER      "/run/wlt/active-partition"

#define WLT_UPDATE_MAX_VERSION        32
#define WLT_UPDATE_MAX_URL            256
#define WLT_UPDATE_MAX_PATH           256
#define WLT_UPDATE_MAX_LINE           512
#define WLT_UPDATE_MAX_HISTORY        16
#define WLT_UPDATE_MAX_HOST           128

#define WLT_UPDATE_CHECK_TIMEOUT_SEC    5   /* spec: check < 5 s           */
#define WLT_UPDATE_DOWNLOAD_TIMEOUT_SEC 600 /* spec: 500 MB < 10 min       */
#define WLT_UPDATE_DOWNLOAD_RETRIES     3
#define WLT_UPDATE_CHUNK_SIZE           (64 * 1024)

#define WLT_UPDATE_ED25519_PUBKEY_LEN 32
#define WLT_UPDATE_ED25519_SIG_LEN    64
#define WLT_UPDATE_MAX_BOOT_ATTEMPTS  3

/* ââ Logging (no shared log.h; emit to stderr) ââ */
#define WLT_UPDATE_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-update][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_UPDATE_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-update][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_UPDATE_LOG_INF(fmt, ...) \
    fprintf(stderr, "[wlt-update][INF] " fmt "\n", ##__VA_ARGS__)
#define WLT_UPDATE_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-update][DBG] " fmt "\n", ##__VA_ARGS__)

/* ââ Types ââ */

/** A version advertised by the repository manifest. */
typedef struct {
    char     version[WLT_UPDATE_MAX_VERSION]; /* e.g. "1.2.3"            */
    uint32_t build;                           /* yyyymmdd build tag       */
    char     timestamp[32];                   /* ISO-8601 release time    */
    char     url[WLT_UPDATE_MAX_URL];         /* .wlt-update.tar.zst URL  */
    uint64_t size;                            /* expected package bytes   */
    char     sig_url[WLT_UPDATE_MAX_URL];     /* detached signature URL   */
    uint64_t rollback_index;                 /* anti-rollback (0 = oldest) */
} wlt_UpdateVersion;

/** Snapshot of update state (for `wltupdate status` / WLT-Settings). */
typedef struct {
    char current_version[WLT_UPDATE_MAX_VERSION];
    char active_partition;            /* 'A' or 'B'                       */
    bool pending_reboot;
    bool update_available;
    char pending_version[WLT_UPDATE_MAX_VERSION];
    uint64_t pending_rollback_index;  /* anti-rollback (0 = unrestricted) */
    char last_update_time[32];
    bool auto_update;
    char last_error[64];
} wlt_UpdateStatus;

/** Update agent configuration (loaded from WLT_UPDATE_CONFIG_PATH). */
typedef struct {
    char     repo_url[WLT_UPDATE_MAX_URL]; /* manifest URL (.onion)        */
    uint16_t tor_socks_port;               /* Tor SOCKS5 port (9050)       */
    char     current_version[WLT_UPDATE_MAX_VERSION];
    bool     auto_update;
    char     pubkey_path[WLT_UPDATE_MAX_PATH];
    char     boot_config_path[WLT_UPDATE_MAX_PATH];
} wlt_UpdateConfig;

/** One entry from the update history log. */
typedef struct {
    char version[WLT_UPDATE_MAX_VERSION];
    char timestamp[32];
    char partition;   /* 'A' or 'B'                                     */
    bool success;     /* true = staged/booted, false = rolled back       */
} wlt_UpdateHistoryEntry;

/* ââ Lifecycle ââ */
wlt_update_error_t wlt_update_init(const wlt_UpdateConfig *config);
void               wlt_update_shutdown(void);

/* ââ Core operations ââ */

/** Check the repo (via Tor) for a newer version. Returns
 *  WLT_UPDATE_ERR_ALREADY_UP_TO_DATE when nothing is newer. */
wlt_update_error_t wlt_update_check(wlt_UpdateVersion *out);

/** Download a package + its signature over Tor (resumable). */
wlt_update_error_t wlt_update_download(const char *dest_path,
                                       const wlt_UpdateVersion *ver);

/** Verify, format the inactive slot, extract, and flip the boot default. */
wlt_update_error_t wlt_update_install(const char *package_path);

wlt_update_error_t wlt_update_status(wlt_UpdateStatus *out);

/** Swap the boot default to the previous, non-failed slot. */
wlt_update_error_t wlt_update_rollback(void);

wlt_update_error_t wlt_update_enable_auto(void);
wlt_update_error_t wlt_update_disable_auto(void);

wlt_update_error_t wlt_update_history(wlt_UpdateHistoryEntry *entries,
                                      size_t max_entries, size_t *count_out);

/* ââ Signature verification (verify.c) ââ */
wlt_update_error_t wlt_update_verify_file(const char *path,
                                          const char *sig_path,
                                          const char *pubkey_path);

/* ─── Signed manifest (research-I R5) ───────────────────────────
 * Binds the package blob to its version, rollback_index, target slot,
 * and SHA-256 hash — all Ed25519-signed. Without this, the signature
 * covers only the raw blob and an attacker can replay an old validly-
 * signed package for a different slot or version (signature replay).
 *
 * The manifest is a small text file alongside the package:
 *   version=1.2.3
 *   rollback_index=42
 *   target_slot=A
 *   sha256=<64 hex chars>
 * The manifest is Ed25519-signed (detached .manifest.sig) with the same
 * key as the blob. verify flow: check manifest sig -> check blob sha256
 * matches manifest -> check version/counter against stored state. */

typedef struct {
    char     version[WLT_UPDATE_MAX_VERSION];
    uint64_t rollback_index;
    char     target_slot;        /* 'A', 'B', or '*' (any) */
    char     sha256_hex[65];     /* 64 hex chars + NUL */
    /* dm-verity (research-I R6): root hash + salt + hash area size for
     * the slot image. When present, the initrd runs `veritysetup open`
     * before switch_root so the active slot is mounted read-only with
     * block-level integrity verification. Generated at build time via
     * `veritysetup format`; the signed manifest binds them so a tampered
     * image cannot be mounted. See research-P-dm-verity in worklog. */
    char     verity_root_hash[65]; /* 64 hex chars + NUL, or empty if none */
    char     verity_salt[65];      /* 64 hex chars + NUL */
    uint64_t verity_size;         /* hash area size in bytes (for --hash-offset) */
} wlt_UpdateManifest;

/* Verify a signed manifest file. Loads + verifies the Ed25519 signature,
 * parses the manifest fields. Returns WLT_UPDATE_ERR_SIGNATURE if the
 * manifest signature is invalid, INVALID_ARG if malformed. */
wlt_update_error_t wlt_update_verify_manifest(const char *manifest_path,
                                              const char *manifest_sig_path,
                                              const char *pubkey_path,
                                              wlt_UpdateManifest *out);

/* Compute the SHA-256 of a file (streamed, for large packages).
 * Writes 64 hex chars + NUL to out_hex (capacity >= 65). */
wlt_update_error_t wlt_update_compute_sha256(const char *path,
                                            char *out_hex, size_t out_hex_size);
wlt_update_error_t wlt_update_verify_buffer(const uint8_t *data, size_t len,
                                            const uint8_t *sig,
                                            const uint8_t *pubkey);

/* ââ Config helpers (config.c; shared internally, no private header) ââ */
void wlt_config_defaults(wlt_UpdateConfig *out);
wlt_update_error_t wlt_config_load(const char *path, wlt_UpdateConfig *out);
wlt_update_error_t wlt_config_save(const char *path,
                                   const wlt_UpdateConfig *cfg);
/** Split one "key = value" line into trimmed outputs. false if no '='. */
bool wlt_config_parse_kv(const char *line,
                         char *key, size_t key_max,
                         char *val, size_t val_max);

/**
 * Bounded string copy (strlcpy semantics). Always NUL-terminates @dst and
 * never overflows. Used in place of strncpy so the build stays clean under
 * -Wstringop-truncation while still satisfying the "no strcpy" rule.
 */
void wlt_strlcpy(char *dst, const char *src, size_t n);

/* ââ Helpers ââ */
const char *wlt_update_error_str(wlt_update_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_UPDATE_H */
