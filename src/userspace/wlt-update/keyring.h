/**
 * @file keyring.h
 * @brief Key chain + rotation for the update signing keys (research-I R7)
 * @purpose Allows multiple Ed25519 signing keys with key-ids + validity
 *          windows, so a compromised key can be retired without reflashing
 *          every device's root key. The signed manifest (R5) names its
 *          signing key_id; verify.c looks up the pubkey in the keyring
 *          instead of loading a single fixed key.
 *
 * Keyring file format (binary, at /etc/wlt/keys/update-keyring.bin):
 *   [u32 magic = 0x574c544b]  ("WLTK")
 *   [u32 entry_count]
 *   for each entry:
 *     [u8  key_id_len]         (1..32)
 *     [u8  key_id[32]]         (NUL-padded)
 *     [u8  pubkey[32]]         (Ed25519)
 *     [u64 not_before]         (epoch seconds; 0 = always)
 *     [u64 not_after]          (epoch seconds; 0 = never expires)
 *
 * @dependencies: libsodium (for crypto_sign_ed25519_verify_detached)
 * @spec: docs/specs/wlt-update.md, research-I R7
 */
#ifndef WLT_KEYRING_H
#define WLT_KEYRING_H

#include <stdint.h>
#include <stdbool.h>
#include "update.h"

#define WLT_KEYRING_MAX_ENTRIES 8
/* WLT_KEYRING_MAX_KEY_ID defined in update.h (32) */

typedef struct {
    char     key_id[WLT_KEYRING_MAX_KEY_ID + 1];
    uint8_t  pubkey[WLT_UPDATE_ED25519_PUBKEY_LEN];  /* 32 bytes */
    uint64_t not_before;  /* epoch seconds; 0 = always valid */
    uint64_t not_after;   /* epoch seconds; 0 = never expires */
    bool     loaded;
} wlt_KeyringEntry;

typedef struct {
    wlt_KeyringEntry entries[WLT_KEYRING_MAX_ENTRIES];
    size_t count;
    bool  loaded;
} wlt_Keyring;

/* Load the keyring from /etc/wlt/keys/update-keyring.bin.
 * If the file doesn't exist, returns WLT_UPDATE_ERR_NOT_FOUND (caller
 * can fall back to the single-key path). On parse error, returns
 * WLT_UPDATE_ERR_INVALID_ARG. */
wlt_update_error_t wlt_keyring_load(wlt_Keyring *kr);

/* Look up a pubkey by key_id. Checks the validity window (not_before/
 * not_after) against the current time. Returns WLT_UPDATE_OK + copies
 * the 32-byte pubkey into out_pubkey if found + valid.
 * Returns WLT_UPDATE_ERR_NOT_FOUND if no matching key_id, or
 * WLT_UPDATE_ERR_INVALID_ARG if the key is expired/not-yet-valid. */
wlt_update_error_t wlt_keyring_lookup(const wlt_Keyring *kr,
                                      const char *key_id,
                                      uint64_t now_epoch,
                                      uint8_t *out_pubkey);

/* Convenience: load + lookup in one call (uses time(NULL) for now). */
wlt_update_error_t wlt_keyring_find(const char *key_id,
                                    uint8_t *out_pubkey);

#endif /* WLT_KEYRING_H */
