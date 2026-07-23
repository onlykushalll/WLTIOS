/**
 * @file keyvault.h
 * @brief IPC client interface to the seL4 Key Vault Protection Domain
 * @module wlt-crypto/keyvault
 * @purpose Sends encrypt/decrypt requests over a Unix socket; keys never
 *          leave the Key Vault PD. All actual crypto happens server-side.
 * @dependencies: crypto.h
 * @tested_by: tests/userspace/keyvault_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * Wire protocol (all integers little-endian, no padding):
 *
 *   Request:
 *     [u32 op]            wlt_kv_op_t
 *     [u32 key_id_len]    Length of key_id (0..WLT_CRYPTO_MAX_KEY_ID)
 *     [u8  *key_id]       key_id bytes (no NUL)
 *     [u32 payload_len]   Length of payload (0..WLT_KEYVAULT_MAX_PAYLOAD)
 *     [u8  *payload]      Plaintext or ciphertext bytes
 *
 *   Response:
 *     [i32 status]        0 = OK, negative = wlt_crypto_error_t value
 *     [u32 payload_len]   Length of returned payload
 *     [u8  *payload]      Ciphertext or plaintext bytes
 *
 * For LIST_KEYS, the response payload is a newline-separated list of
 * key IDs. For GET_VOLUME_KEY, the payload is the raw key bytes
 * (transient use only — caller MUST sodium_memzero immediately after
 * passing the key to the kernel).
 */

#ifndef WLT_KEYVAULT_H
#define WLT_KEYVAULT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ──────────────────────────────────────── */

#define WLT_KEYVAULT_SOCKET        "/run/key-vault.sock"
#define WLT_KEYVAULT_MAX_PAYLOAD   (16 * 1024 * 1024)
#define WLT_KEYVAULT_TIMEOUT_MS    1000
#define WLT_KEYVAULT_CONNECT_TRIES 3

/* Volume key size for the dm-crypt cipher (aes-xts-plain64, 512-bit). */
#define WLT_KEYVAULT_VOLUME_KEY_LEN  64
#define WLT_KEYVAULT_VOLUME_KEY_HEX  (WLT_KEYVAULT_VOLUME_KEY_LEN * 2 + 1)

/* ─── IPC Operations ─────────────────────────────────── */

typedef enum {
    WLT_KV_OP_PING = 1,            /* health check, no payload          */
    WLT_KV_OP_ENCRYPT = 2,         /* payload=plaintext, resp=cipher    */
    WLT_KV_OP_DECRYPT = 3,         /* payload=cipher,  resp=plaintext   */
    WLT_KV_OP_GENERATE_KEY = 4,    /* payload=type,    resp=key_id      */
    WLT_KV_OP_DELETE_KEY = 5,      /* payload=key_id in key_id field    */
    WLT_KV_OP_LIST_KEYS = 6,       /* resp=newline-separated key IDs    */
    WLT_KV_OP_GET_VOLUME_KEY = 7,  /* payload=key_id, resp=raw key      */
} wlt_kv_op_t;

/* ─── Key Types (for GENERATE_KEY) ───────────────────── */

typedef enum {
    WLT_KV_KEY_FILE = 1,           /* Per-file encryption               */
    WLT_KV_KEY_LUKS = 2,           /* LUKS master key wrapping          */
    WLT_KV_KEY_TOR = 3,            /* Tor long-term identity            */
    WLT_KV_KEY_PACKAGE = 4,        /* Package signature verification    */
    WLT_KV_KEY_UPDATE = 5,         /* Update signature verification     */
    WLT_KV_KEY_AI = 6,             /* AI conversation memory            */
} wlt_kv_key_type_t;

/* ─── Lifecycle ──────────────────────────────────────── */

/**
 * Connect to the Key Vault PD. Idempotent — safe to call repeatedly.
 * Returns WLT_CRYPTO_OK if already connected. Retries the connect()
 * a few times to tolerate the PD still starting during early boot.
 */
wlt_crypto_error_t wlt_keyvault_init(void);

/**
 * Close the IPC socket. Safe to call when not connected.
 */
void wlt_keyvault_shutdown(void);

/**
 * Verify the Key Vault is reachable and responsive.
 * Used by WLT-Init at boot to decide whether to continue.
 */
wlt_crypto_error_t wlt_keyvault_ping(void);

/* ─── Crypto Operations ──────────────────────────────── */

/**
 * Encrypt plaintext using the named key. The actual encryption happens
 * inside the Key Vault PD; WLT-Crypto never sees the key.
 *
 * @param key_id          Key identifier known to the PD
 * @param plaintext       Input bytes
 * @param plaintext_len   Input length (0..WLT_KEYVAULT_MAX_PAYLOAD)
 * @param ciphertext      Output buffer (>= plaintext_len + 40)
 * @param ciphertext_len  In: buffer capacity. Out: bytes written.
 */
wlt_crypto_error_t wlt_keyvault_encrypt(const char *key_id,
                                        const uint8_t *plaintext,
                                        size_t plaintext_len,
                                        uint8_t *ciphertext,
                                        size_t *ciphertext_len);

/**
 * Decrypt ciphertext previously produced by wlt_keyvault_encrypt.
 * On failure the plaintext buffer is zeroed so no partial output leaks.
 */
wlt_crypto_error_t wlt_keyvault_decrypt(const char *key_id,
                                        const uint8_t *ciphertext,
                                        size_t ciphertext_len,
                                        uint8_t *plaintext,
                                        size_t *plaintext_len);

/**
 * Ask the PD to generate a new key of the given type.
 *
 * @param type         Key purpose
 * @param key_id_out   Buffer to receive the new key ID (NUL-terminated)
 * @param key_id_size  Size of key_id_out (>= WLT_CRYPTO_MAX_KEY_ID)
 */
wlt_crypto_error_t wlt_keyvault_generate_key(wlt_kv_key_type_t type,
                                             char *key_id_out,
                                             size_t key_id_size);

/**
 * Permanently delete a key from the PD.
 */
wlt_crypto_error_t wlt_keyvault_delete_key(const char *key_id);

/**
 * List all known key IDs (NOT the keys themselves).
 *
 * @param buf      Output buffer (NUL-terminated on success)
 * @param buf_size Size of buf
 * @param written  Number of bytes written (excluding NUL)
 */
wlt_crypto_error_t wlt_keyvault_list_keys(char *buf, size_t buf_size,
                                          size_t *written);

/**
 * Retrieve a raw volume key for dm-crypt setup.
 *
 * SECURITY: This is the one operation that places key material in
 * WLT-Crypto's address space, because dm-crypt requires the key in
 * kernel memory via ioctl. The caller MUST zero the returned buffer
 * immediately after passing it to the kernel.
 *
 * @param key_id   Key ID whose volume key is requested
 * @param key_out  Output buffer (>= WLT_KEYVAULT_VOLUME_KEY_LEN bytes)
 * @param key_len  In: buffer capacity. Out: bytes written.
 */
wlt_crypto_error_t wlt_keyvault_get_volume_key(const char *key_id,
                                               uint8_t *key_out,
                                               size_t *key_len);

/* Get the LUKS volume key as a file descriptor (pipe read end).
 *
 * This is the integration point for research-H's bridge architecture:
 * the bridge daemon (wlt-keyvault-bridge) connects to the seL4 KeyVaultPD
 * over a Microkit channel + virtio-console, and exposes the key via an
 * AF_UNIX socket at /run/wlt-keyvault.sock. systemd-cryptsetup can read
 * the key directly from this socket (its built-in AF_UNIX keyfile feature),
 * OR luks.c can read the bytes from the returned fd and write them to a
 * per-call keyfile (the current luks.c approach).
 *
 * When the bridge is not running (no KeyVaultPD), this returns
 * WLT_CRYPTO_ERR_KEYVAULT — fail-closed, never falls back to a hardcoded key.
 *
 * @param key_id   Key ID whose volume key is requested
 * @param fd_out   Output: pipe read-end fd with the key bytes (64 bytes).
 *                 Caller MUST close(fd) after use (luks.c does this via
 *                 the keyfile write loop + remove_keyfile).
 * @returns WLT_CRYPTO_OK on success, KEYVAULT if bridge unavailable. */
wlt_crypto_error_t wlt_keyvault_get_volume_key_fd(const char *key_id,
                                                  int *fd_out);

#ifdef __cplusplus
}
#endif

#endif /* WLT_KEYVAULT_H */
