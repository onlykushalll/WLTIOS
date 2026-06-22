/**
 * @file verify.c
 * @brief Ed25519 signature verification for update packages
 * @module wlt-update/verify
 * @purpose Refuses any update whose detached Ed25519 signature does not
 *          validate against the trusted update public key. Verification
 *          is streamed so 500 MB packages never need to fit in RAM.
 * @dependencies: libsodium (crypto_sign_ed25519_*), update.h
 * @tested_by: tests/userspace/wlt_verify_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/06-musl-libc.md
 *
 * NOTE: libsodium is REQUIRED for genuine Ed25519. When the build host
 * lacks the dev headers (HAVE_SODIUM undefined), every entry point
 * returns WLT_UPDATE_ERR_UNSUPPORTED so callers degrade loudly rather
 * than silently skipping verification â a missing crypto library must
 * never be confused with a valid signature.
 */

#include "update.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

/* âââ Internal: load a raw 32-byte public key ââââââââââ
 * The key file is exactly WLT_UPDATE_ED25519_PUBKEY_LEN bytes. We
 * deliberately reject anything larger or smaller: an off-by-one key
 * would verify against the wrong material. Only needed when libsodium
 * is linked, hence guarded to stay warning-clean without it.
 */
#ifdef HAVE_SODIUM
static wlt_update_error_t
load_pubkey(const char *path, uint8_t *out)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        WLT_UPDATE_LOG_ERR("cannot open pubkey: %s", path);
        return WLT_UPDATE_ERR_NOT_FOUND;
    }

    size_t got = fread(out, 1, WLT_UPDATE_ED25519_PUBKEY_LEN, fp);
    /* Drain the file to detect trailing garbage. */
    uint8_t extra;
    if (fread(&extra, 1, 1, fp) > 0)
    {
        got = 0; /* force the size check below to fail */
    }
    fclose(fp);

    if (got != WLT_UPDATE_ED25519_PUBKEY_LEN)
    {
        WLT_UPDATE_LOG_ERR("pubkey %s is %zu bytes, expected %d",
                           path, got, WLT_UPDATE_ED25519_PUBKEY_LEN);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }
    return WLT_UPDATE_OK;
}

/* âââ Internal: load a raw 64-byte detached signature ââ */
static wlt_update_error_t
load_signature(const char *path, uint8_t *out)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        WLT_UPDATE_LOG_ERR("cannot open signature: %s", path);
        return WLT_UPDATE_ERR_NOT_FOUND;
    }

    size_t got = fread(out, 1, WLT_UPDATE_ED25519_SIG_LEN, fp);
    uint8_t extra;
    if (fread(&extra, 1, 1, fp) > 0)
    {
        got = 0;
    }
    fclose(fp);

    if (got != WLT_UPDATE_ED25519_SIG_LEN)
    {
        WLT_UPDATE_LOG_ERR("signature %s is %zu bytes, expected %d",
                           path, got, WLT_UPDATE_ED25519_SIG_LEN);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }
    return WLT_UPDATE_OK;
}
#endif /* HAVE_SODIUM */

/* âââ Public: verify an in-memory buffer âââââââââââââââ */
wlt_update_error_t
wlt_update_verify_buffer(const uint8_t *data,
                         size_t len,
                         const uint8_t *sig,
                         const uint8_t *pubkey)
{
    if (data == NULL && len > 0)
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }
    if (sig == NULL || pubkey == NULL)
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

#ifdef HAVE_SODIUM
    if (sodium_init() < 0)
    {
        WLT_UPDATE_LOG_ERR("libsodium failed to initialise");
        return WLT_UPDATE_ERR_UNSUPPORTED;
    }

    /* crypto_sign_verify_detached returns 0 on a valid signature. */
    if (crypto_sign_ed25519_verify_detached(sig, data, len, pubkey) != 0)
    {
        WLT_UPDATE_LOG_ERR("signature verification FAILED");
        return WLT_UPDATE_ERR_SIGNATURE;
    }
    return WLT_UPDATE_OK;
#else
    (void)data;
    (void)len;
    (void)sig;
    (void)pubkey;
    WLT_UPDATE_LOG_ERR("built without libsodium â verification refused");
    return WLT_UPDATE_ERR_UNSUPPORTED;
#endif
}

/* âââ Public: stream-verify a file âââââââââââââââââââââ
 * Uses libsodium's multi-part Ed25519 API (crypto_sign_init/update/
 * final_verify) so the whole package is never resident in memory.
 */
wlt_update_error_t
wlt_update_verify_file(const char *path,
                       const char *sig_path,
                       const char *pubkey_path)
{
    if (path == NULL || sig_path == NULL || pubkey_path == NULL)
    {
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

#ifndef HAVE_SODIUM
    WLT_UPDATE_LOG_ERR("built without libsodium â verification refused");
    return WLT_UPDATE_ERR_UNSUPPORTED;
#else
    uint8_t pubkey[WLT_UPDATE_ED25519_PUBKEY_LEN];
    uint8_t sig[WLT_UPDATE_ED25519_SIG_LEN];

    wlt_update_error_t err = load_pubkey(pubkey_path, pubkey);
    if (err != WLT_UPDATE_OK)
    {
        return err;
    }
    err = load_signature(sig_path, sig);
    if (err != WLT_UPDATE_OK)
    {
        return err;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
    {
        WLT_UPDATE_LOG_ERR("cannot open package: %s", path);
        return WLT_UPDATE_ERR_NOT_FOUND;
    }

    if (sodium_init() < 0)
    {
        fclose(fp);
        return WLT_UPDATE_ERR_UNSUPPORTED;
    }

    /* crypto_sign_state is large; keep it on the stack of this leaf. */
    crypto_sign_state st;
    if (crypto_sign_init(&st) != 0)
    {
        fclose(fp);
        return WLT_UPDATE_ERR_IO;
    }

    uint8_t *buf = malloc(WLT_UPDATE_CHUNK_SIZE);
    if (buf == NULL)
    {
        fclose(fp);
        return WLT_UPDATE_ERR_NOMEM;
    }

    err = WLT_UPDATE_OK;
    for (;;)
    {
        size_t n = fread(buf, 1, WLT_UPDATE_CHUNK_SIZE, fp);
        if (n == 0)
        {
            if (ferror(fp))
            {
                WLT_UPDATE_LOG_ERR("read error on %s", path);
                err = WLT_UPDATE_ERR_IO;
            }
            break; /* EOF or error handled above */
        }
        if (crypto_sign_update(&st, buf, n) != 0)
        {
            WLT_UPDATE_LOG_ERR("hash update failed on %s", path);
            err = WLT_UPDATE_ERR_IO;
            break;
        }
    }

    fclose(fp);
    free(buf);

    if (err != WLT_UPDATE_OK)
    {
        return err;
    }

    /* final_verify returns 0 iff the signature is valid. */
    if (crypto_sign_final_verify(&st, sig, pubkey) != 0)
    {
        WLT_UPDATE_LOG_ERR("signature verification FAILED for %s", path);
        return WLT_UPDATE_ERR_SIGNATURE;
    }

    WLT_UPDATE_LOG_DBG("signature OK for %s", path);
    return WLT_UPDATE_OK;
#endif
}
