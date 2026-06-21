/**
 * @file crypto.h
 * @brief Public API for WLT-Crypto (disk + file encryption layer)
 * @module wlt-crypto
 * @purpose Bridges apps that need crypto to the seL4 Key Vault PD that
 *          holds the keys. Keys never enter this process's address space
 *          except for the transient volume-key path used by dm-crypt.
 * @dependencies: keyvault.h, file_crypto.h, libsodium (optional)
 * @tested_by: tests/userspace/crypto_test.c
 * @spec: docs/specs/wlt-crypto.md
 * @adr: docs/adr/02-sel4-as-base.md, docs/adr/06-musl-libc.md
 */

#ifndef WLT_CRYPTO_H
#define WLT_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ──────────────────────────────────────
 * Every WLT-Crypto function returns one of these. Negative values are
 * errors; zero (WLT_CRYPTO_OK) is the only success code.
 */
typedef enum {
    WLT_CRYPTO_OK = 0,
    WLT_CRYPTO_ERR_INVALID_ARG = -1,     /* NULL ptr, bad length, etc.   */
    WLT_CRYPTO_ERR_NOT_FOUND = -2,       /* File or device missing       */
    WLT_CRYPTO_ERR_PERMISSION = -3,      /* Not authorised               */
    WLT_CRYPTO_ERR_IO = -4,              /* read/write/lock failed       */
    WLT_CRYPTO_ERR_NOMEM = -5,           /* malloc failed                */
    WLT_CRYPTO_ERR_KEYVAULT = -6,        /* Key Vault PD unreachable     */
    WLT_CRYPTO_ERR_KEY_NOT_FOUND = -7,   /* Unknown key ID               */
    WLT_CRYPTO_ERR_DECRYPT = -8,         /* Decrypt failed (wrong key)   */
    WLT_CRYPTO_ERR_LUKS = -9,            /* LUKS operation failed        */
    WLT_CRYPTO_ERR_LUKS_HEADER = -10,    /* Corrupt LUKS2 header         */
    WLT_CRYPTO_ERR_TIMEOUT = -11,        /* Operation timed out          */
    WLT_CRYPTO_ERR_FORMAT = -12,         /* File format invalid          */
    WLT_CRYPTO_ERR_TOO_LARGE = -13,      /* Buffer/file too large        */
    WLT_CRYPTO_ERR_UNSUPPORTED = -14,    /* Operation not supported      */
    WLT_CRYPTO_ERR_ALREADY_EXISTS = -15, /* Mapper name already in use   */
} wlt_crypto_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_CRYPTO_VERSION            "1.0.0"
#define WLT_CRYPTO_MAX_KEY_ID         64
#define WLT_CRYPTO_MAX_PATH           256
#define WLT_CRYPTO_MAX_LUKS_NAME      32
#define WLT_CRYPTO_MAX_DEVICE_PATH    128

/* Maximum buffer size for in-memory encrypt/decrypt (16 MiB).
 * Larger payloads must use the streaming file API. */
#define WLT_CRYPTO_MAX_BUFFER_SIZE    (16 * 1024 * 1024)

/* Per-message ciphertext overhead: 24-byte nonce + 16-byte Poly1305 MAC.
 * Matches the XSalsa20-Poly1305 layout the Key Vault PD emits. */
#define WLT_CRYPTO_CIPHERTEXT_OVERHEAD 40

/* Default streaming chunk size for file encryption (64 KiB). */
#define WLT_CRYPTO_DEFAULT_CHUNK_SIZE  (64 * 1024)

/* Key Vault IPC retry count for transient timeouts (spec: retry 3x). */
#define WLT_CRYPTO_IPC_RETRIES         3

/* Default shred pass count: random, 0x00, 0xFF. */
#define WLT_CRYPTO_SHRED_DEFAULT_PASSES 3

/* ─── Logging ──────────────────────────────────────────
 * WLT-Crypto has no shared log.h; modules emit to stderr. The init
 * system may redirect stderr to a log file. Macros keep the call
 * sites concise and guarantee a consistent "[wlt-crypto][LVL]" prefix.
 */
#define WLT_CRYPTO_LOG_ERR(fmt, ...) \
    fprintf(stderr, "[wlt-crypto][ERR] " fmt "\n", ##__VA_ARGS__)
#define WLT_CRYPTO_LOG_WRN(fmt, ...) \
    fprintf(stderr, "[wlt-crypto][WRN] " fmt "\n", ##__VA_ARGS__)
#define WLT_CRYPTO_LOG_INF(fmt, ...) \
    fprintf(stderr, "[wlt-crypto][INF] " fmt "\n", ##__VA_ARGS__)
#define WLT_CRYPTO_LOG_DBG(fmt, ...) \
    fprintf(stderr, "[wlt-crypto][DBG] " fmt "\n", ##__VA_ARGS__)

/* ─── Lifecycle ──────────────────────────────────────── */

/**
 * Initialise WLT-Crypto. Must be called once before any other function.
 * Connects to the Key Vault PD via the Unix socket at
 * WLT_KEYVAULT_SOCKET and confirms it responds to a ping. Returns
 * WLT_CRYPTO_ERR_KEYVAULT if the PD is unreachable — per spec, all
 * crypto operations MUST be refused in that state.
 */
wlt_crypto_error_t wlt_crypto_init(void);

/**
 * Shutdown WLT-Crypto. Closes the Key Vault IPC socket.
 * Safe to call multiple times and when init was never called.
 */
void wlt_crypto_shutdown(void);

/* ─── Buffer Encrypt/Decrypt ─────────────────────────── */

/**
 * Encrypt a buffer. Actual encryption happens inside the Key Vault PD;
 * the key never leaves that partition. The ciphertext layout is
 * nonce(24) || MAC(16) || encrypted, so out_len on success is
 * len + WLT_CRYPTO_CIPHERTEXT_OVERHEAD.
 *
 * @param plaintext   Input bytes (may be NULL only if len == 0)
 * @param len         Plaintext length (<= WLT_CRYPTO_MAX_BUFFER_SIZE)
 * @param key_id      Key ID known to the Key Vault PD
 * @param ciphertext  Output buffer (>= len + WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
 * @param out_len     In: capacity of ciphertext. Out: bytes written.
 */
wlt_crypto_error_t wlt_crypto_encrypt(const uint8_t *plaintext, size_t len,
                                      const char *key_id,
                                      uint8_t *ciphertext, size_t *out_len);

/**
 * Decrypt a buffer previously encrypted with wlt_crypto_encrypt.
 * On failure the plaintext buffer is zeroed so no partial output leaks.
 */
wlt_crypto_error_t wlt_crypto_decrypt(const uint8_t *ciphertext, size_t len,
                                      const char *key_id,
                                      uint8_t *plaintext, size_t *out_len);

/* ─── File Encrypt/Decrypt ───────────────────────────── */

/**
 * Encrypt a file in-place using streaming chunked encryption. Writes a
 * temp file then atomically renames over the original. The original
 * plaintext file is destroyed by the rename.
 *
 * @param path    File to encrypt (replaced in-place)
 * @param key_id  Key ID known to the Key Vault PD
 */
wlt_crypto_error_t wlt_crypto_encrypt_file(const char *path,
                                           const char *key_id);

/**
 * Decrypt a WLT1-format file in-place. If key_id is NULL, the key ID
 * stored in the file header is used. A supplied key_id must match the
 * header or the operation is refused (possible attack).
 */
wlt_crypto_error_t wlt_crypto_decrypt_file(const char *path,
                                           const char *key_id);

/* ─── LUKS Disk Encryption ─────────────────────────────
 * These are part of the public API per spec. The actual LUKS2 / dm-crypt
 * implementation lives in luks.c and dm_crypt.c (not part of this
 * 8-file build). When those modules are not linked, the dispatchers in
 * crypto.c return WLT_CRYPTO_ERR_UNSUPPORTED so callers can degrade
 * gracefully.
 */

wlt_crypto_error_t wlt_crypto_luks_format(const char *device,
                                          const char *key_id);
wlt_crypto_error_t wlt_crypto_luks_open(const char *device,
                                        const char *name,
                                        const char *key_id);
wlt_crypto_error_t wlt_crypto_luks_close(const char *name);
wlt_crypto_error_t wlt_crypto_luks_status(const char *name,
                                          bool *is_open);

/* ─── Secure Delete ──────────────────────────────────── */

/**
 * Securely delete a file by overwriting it multiple times then
 * unlinking. Default pattern (3 passes): random, 0x00, 0xFF, then a
 * final zero pass and ftruncate to 0.
 *
 * @param path   File to shred (must be a regular file, not a symlink)
 * @param passes Number of overwrite passes (0 = use default of 3)
 */
wlt_crypto_error_t wlt_crypto_shred(const char *path, int passes);

/* ─── Utility ────────────────────────────────────────── */

/**
 * Convert an error code to a human-readable string. Never returns NULL.
 */
const char *wlt_crypto_error_str(wlt_crypto_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_CRYPTO_H */
