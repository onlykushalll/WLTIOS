/**
 * @file file_crypto.h
 * @brief Streaming per-file encryption (XSalsa20-Poly1305 via Key Vault PD)
 * @module wlt-crypto/file_crypto
 * @purpose Encrypts and decrypts files chunk-by-chunk, dispatching each
 *          chunk to the Key Vault PD for the actual cryptographic work.
 *          The file format is "WLT1" — a small binary header followed
 *          by length-prefixed ciphertext chunks.
 * @dependencies: crypto.h, keyvault.h, libsodium (optional)
 * @tested_by: tests/userspace/file_crypto_test.c
 * @spec: docs/specs/wlt-crypto.md
 *
 * File layout (all integers little-endian):
 *   [128 bytes header]
 *     +0    4   magic "WLT1"
 *     +4    1   version (1)
 *     +5    1   flags  (bit0 = file_len authoritative)
 *     +6    2   reserved (0)
 *     +8    64  key_id, NUL-padded
 *     +72   4   key_id_len
 *     +76   4   chunk_size used during encryption
 *     +80   8   original file length (0 if unknown)
 *     +88   24  per-file nonce (reserved for future secretstream use)
 *     +112  16  padding (0)
 *   Then chunks: [u32 cipher_len][cipher bytes]... [u32 0 = end marker]
 */

#ifndef WLT_FILE_CRYPTO_H
#define WLT_FILE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include "crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── File Format Constants ──────────────────────────── */

#define WLT_FILE_CRYPTO_MAGIC         "WLT1"
#define WLT_FILE_CRYPTO_MAGIC_LEN     4
#define WLT_FILE_CRYPTO_VERSION       1
#define WLT_FILE_CRYPTO_HEADER_SIZE   128

/* Header layout offsets. */
#define WLT_FILE_CRYPTO_OFF_MAGIC       0
#define WLT_FILE_CRYPTO_OFF_VERSION     4
#define WLT_FILE_CRYPTO_OFF_FLAGS       5
#define WLT_FILE_CRYPTO_OFF_RESERVED    6
#define WLT_FILE_CRYPTO_OFF_KEY_ID      8
#define WLT_FILE_CRYPTO_OFF_KEY_ID_LEN  72
#define WLT_FILE_CRYPTO_OFF_CHUNK_SIZE  76
#define WLT_FILE_CRYPTO_OFF_FILE_LEN    80
#define WLT_FILE_CRYPTO_OFF_NONCE       88
#define WLT_FILE_CRYPTO_OFF_PADDING     112

#define WLT_FILE_CRYPTO_NONCE_LEN    24
#define WLT_FILE_CRYPTO_MAC_LEN      16

/* Flags byte (offset 5). Bit 0 = file length is authoritative. */
#define WLT_FILE_CRYPTO_FLAG_HAS_LENGTH 0x01

/* End-of-stream marker. Written as a 4-byte LE zero after the final
 * chunk. Any non-zero length read in-stream is a chunk. */
#define WLT_FILE_CRYPTO_END_MARKER   0u

/* ─── Public API ─────────────────────────────────────── */

/**
 * Encrypt a file in-place using streaming chunked encryption.
 * Writes a temp file then atomically renames over the original.
 *
 * @param path    File to encrypt (replaced in-place)
 * @param key_id  Key ID known to the Key Vault PD
 */
wlt_crypto_error_t wlt_file_crypto_encrypt_file(const char *path,
                                                const char *key_id);

/**
 * Decrypt a WLT1-format file in-place. If key_id is NULL, the key ID
 * stored in the file header is used. A supplied key_id must match the
 * header or the operation is refused (possible attack).
 */
wlt_crypto_error_t wlt_file_crypto_decrypt_file(const char *path,
                                                const char *key_id);

#ifdef __cplusplus
}
#endif

#endif /* WLT_FILE_CRYPTO_H */
