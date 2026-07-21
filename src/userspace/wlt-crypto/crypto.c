/**
 * @file crypto.c
 * @brief Main entry point and dispatcher for WLT-Crypto
 * @module wlt-crypto/core
 * @purpose Public API surface; routes calls to keyvault, file_crypto,
 *          and secure_delete submodules. Enforces input validation
 *          and centralises error translation. The Key Vault PD must
 *          be reachable before any crypto operation is permitted.
 * @dependencies: crypto.h, keyvault.h, file_crypto.h, secure_delete.c
 * @tested_by: tests/userspace/crypto_test.c
 * @spec: docs/specs/wlt-crypto.md
 * @adr: docs/adr/02-sel4-as-base.md, docs/adr/06-musl-libc.md
 */

#include "crypto.h"
#include "keyvault.h"
#include "file_crypto.h"

#include <string.h>

/* ─── Forward Declarations ─────────────────────────────
 * secure_delete.c is part of this build but has no public header; the
 * impl is referenced here as an internal contract.
 */
wlt_crypto_error_t wlt_secure_delete_impl(const char *path, int passes);

/* ─── Module State ───────────────────────────────────── */

static bool g_crypto_initialized = false;

/* ─── Internal: Input Validation ─────────────────────── */

static bool
valid_key_id(const char *key_id)
{
    if (key_id == NULL)
        return false;
    size_t len = strlen(key_id);
    if (len == 0 || len >= WLT_CRYPTO_MAX_KEY_ID)
        return false;
    /* Disallow whitespace or control chars — these would corrupt the
     * wire protocol's length-prefixed framing. */
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)key_id[i];
        if (c <= 0x20 || c == 0x7f)
            return false;
    }
    return true;
}

static bool
valid_path(const char *path)
{
    if (path == NULL)
        return false;
    size_t len = strlen(path);
    if (len == 0 || len >= WLT_CRYPTO_MAX_PATH)
        return false;
    return true;
}

static bool
valid_device(const char *device)
{
    if (device == NULL)
        return false;
    size_t len = strlen(device);
    if (len == 0 || len >= WLT_CRYPTO_MAX_DEVICE_PATH)
        return false;
    return true;
}

static bool
valid_luks_name(const char *name)
{
    if (name == NULL)
        return false;
    size_t len = strlen(name);
    if (len == 0 || len >= WLT_CRYPTO_MAX_LUKS_NAME)
        return false;
    return true;
}

/* ─── Lifecycle ──────────────────────────────────────── */

wlt_crypto_error_t
wlt_crypto_init(void)
{
    if (g_crypto_initialized)
        return WLT_CRYPTO_OK;

    /* Connect to the Key Vault PD. Refuse to operate without it. */
    wlt_crypto_error_t err = wlt_keyvault_init();
    if (err != WLT_CRYPTO_OK)
    {
        WLT_CRYPTO_LOG_ERR("init: cannot reach Key Vault PD: %s",
                           wlt_crypto_error_str(err));
        return err;
    }

    /* Confirm the PD is actually serving requests. */
    err = wlt_keyvault_ping();
    if (err != WLT_CRYPTO_OK)
    {
        WLT_CRYPTO_LOG_ERR("init: Key Vault PD not responding: %s",
                           wlt_crypto_error_str(err));
        wlt_keyvault_shutdown();
        return err;
    }

    g_crypto_initialized = true;
    WLT_CRYPTO_LOG_INF("wlt-crypto v%s ready", WLT_CRYPTO_VERSION);
    return WLT_CRYPTO_OK;
}

void
wlt_crypto_shutdown(void)
{
    if (!g_crypto_initialized)
        return;
    wlt_keyvault_shutdown();
    g_crypto_initialized = false;
    WLT_CRYPTO_LOG_INF("wlt-crypto shutdown");
}

/* ─── Buffer Encrypt/Decrypt ─────────────────────────── */

wlt_crypto_error_t
wlt_crypto_encrypt(const uint8_t *plaintext, size_t len,
                   const char *key_id,
                   uint8_t *ciphertext, size_t *out_len)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (ciphertext == NULL || out_len == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (len > 0 && plaintext == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (len > WLT_CRYPTO_MAX_BUFFER_SIZE)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    /* Caller must provide room for nonce + MAC overhead. */
    if (*out_len < len + WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    wlt_crypto_error_t err = wlt_keyvault_encrypt(key_id, plaintext, len,
                                                  ciphertext, out_len);
    if (err != WLT_CRYPTO_OK)
    {
        WLT_CRYPTO_LOG_ERR("encrypt: keyvault failure for key '%s': %s",
                           key_id, wlt_crypto_error_str(err));
    }
    return err;
}

wlt_crypto_error_t
wlt_crypto_decrypt(const uint8_t *ciphertext, size_t len,
                   const char *key_id,
                   uint8_t *plaintext, size_t *out_len)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (ciphertext == NULL || plaintext == NULL || out_len == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (!valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (len < WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        return WLT_CRYPTO_ERR_FORMAT;
    if (len > WLT_CRYPTO_MAX_BUFFER_SIZE + WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    /* Plaintext cannot be larger than ciphertext minus overhead. */
    if (*out_len < len - WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    wlt_crypto_error_t err = wlt_keyvault_decrypt(key_id, ciphertext, len,
                                                  plaintext, out_len);
    if (err != WLT_CRYPTO_OK)
    {
        /* Per spec: log decrypt failures as possible attacks. */
        WLT_CRYPTO_LOG_ERR("decrypt: FAILED for key '%s' "
                           "(possible attack): %s",
                           key_id, wlt_crypto_error_str(err));
    }
    return err;
}

/* ─── File Encrypt/Decrypt ───────────────────────────── */

wlt_crypto_error_t
wlt_crypto_encrypt_file(const char *path, const char *key_id)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (!valid_path(path) || !valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;

    return wlt_file_crypto_encrypt_file(path, key_id);
}

wlt_crypto_error_t
wlt_crypto_decrypt_file(const char *path, const char *key_id)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (!valid_path(path))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    /* key_id may be NULL — file_crypto reads it from the header. */
    if (key_id != NULL && !valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;

    return wlt_file_crypto_decrypt_file(path, key_id);
}

/* ─── LUKS Disk Encryption (dispatch) ──────────────────
 * The LUKS2 + dm-crypt implementation lives in luks.c and dm_crypt.c,
 * which are NOT part of this 8-file build. When those modules are not
 * linked, the dispatchers validate input then return UNSUPPORTED so
 * callers can degrade gracefully. Linking luks.c replaces these
 * stubs with the real wlt_luks_*_impl forwarding.
 */

wlt_crypto_error_t
wlt_crypto_luks_format(const char *device, const char *key_id)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (!valid_device(device) || !valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    WLT_CRYPTO_LOG_ERR("luks_format: not built (requires luks.c module)");
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_crypto_luks_open(const char *device, const char *name,
                     const char *key_id)
{
    if (!g_crypto_initialized)
        return WLT_CRYPTO_ERR_KEYVAULT;
    if (!valid_device(device) || !valid_luks_name(name) ||
        !valid_key_id(key_id))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    WLT_CRYPTO_LOG_ERR("luks_open: not built (requires luks.c module)");
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_crypto_luks_close(const char *name)
{
    if (!valid_luks_name(name))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    WLT_CRYPTO_LOG_ERR("luks_close: not built (requires luks.c module)");
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_crypto_luks_status(const char *name, bool *is_open)
{
    if (!valid_luks_name(name) || is_open == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    /* No PD round-trip needed: report "not open" when the LUKS module
     * is absent so boot logic does not block on a phantom mapper. */
    *is_open = false;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

/* ─── Secure Delete ──────────────────────────────────── */

wlt_crypto_error_t
wlt_crypto_shred(const char *path, int passes)
{
    if (!valid_path(path))
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (passes < 0)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (passes == 0)
        passes = WLT_CRYPTO_SHRED_DEFAULT_PASSES;

    return wlt_secure_delete_impl(path, passes);
}

/* ─── Error Strings ──────────────────────────────────── */

const char *
wlt_crypto_error_str(wlt_crypto_error_t err)
{
    switch (err)
    {
        case WLT_CRYPTO_OK:                  return "ok";
        case WLT_CRYPTO_ERR_INVALID_ARG:     return "invalid argument";
        case WLT_CRYPTO_ERR_NOT_FOUND:       return "not found";
        case WLT_CRYPTO_ERR_PERMISSION:      return "permission denied";
        case WLT_CRYPTO_ERR_IO:              return "I/O error";
        case WLT_CRYPTO_ERR_NOMEM:           return "out of memory";
        case WLT_CRYPTO_ERR_KEYVAULT:        return "key vault unavailable";
        case WLT_CRYPTO_ERR_KEY_NOT_FOUND:   return "key not found";
        case WLT_CRYPTO_ERR_DECRYPT:         return "decryption failed";
        case WLT_CRYPTO_ERR_LUKS:            return "LUKS error";
        case WLT_CRYPTO_ERR_LUKS_HEADER:     return "corrupt LUKS header";
        case WLT_CRYPTO_ERR_TIMEOUT:         return "operation timed out";
        case WLT_CRYPTO_ERR_FORMAT:          return "invalid file format";
        case WLT_CRYPTO_ERR_TOO_LARGE:       return "buffer too large";
        case WLT_CRYPTO_ERR_UNSUPPORTED:     return "unsupported operation";
        case WLT_CRYPTO_ERR_ALREADY_EXISTS:  return "already exists";
        default:                             return "unknown error";
    }
}
