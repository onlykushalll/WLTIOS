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

/* ─── Signed manifest (research-I R5) ───────────────────────────
 * The functions below implement the signed-manifest verification that
 * binds an Ed25519 signature to {version, rollback_index, target_slot,
 * sha256(blob)} so a validly-signed package cannot be replayed against
 * a different slot or a rolled-back version. */

/* Parse a manifest KV file (same format as the manifest in update.c).
 * Lines: version=X, rollback_index=N, target_slot=A, sha256=<hex>. */
static wlt_update_error_t
parse_manifest_file(FILE *fp, wlt_UpdateManifest *out)
{
    char line[WLT_UPDATE_MAX_LINE];
    char key[WLT_UPDATE_MAX_LINE];
    char val[WLT_UPDATE_MAX_LINE];
    memset(out, 0, sizeof(*out));
    out->target_slot = '*';  /* default: any slot */

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (!wlt_config_parse_kv(line, key, sizeof(key), val, sizeof(val)))
            continue;
        if (strcmp(key, "version") == 0)
            wlt_strlcpy(out->version, val, sizeof(out->version));
        else if (strcmp(key, "rollback_index") == 0)
            out->rollback_index = strtoull(val, NULL, 10);
        else if (strcmp(key, "target_slot") == 0)
            out->target_slot = val[0];
        else if (strcmp(key, "sha256") == 0)
            wlt_strlcpy(out->sha256_hex, val, sizeof(out->sha256_hex));
    }
    if (out->version[0] == '\0' || out->sha256_hex[0] == '\0')
        return WLT_UPDATE_ERR_INVALID_ARG;
    return WLT_UPDATE_OK;
}

/* Verify the manifest's own Ed25519 signature, then parse it.
 * The manifest is small (a few hundred bytes) so we read it whole
 * into a stack buffer and use crypto_sign_verify_detached (not the
 * streaming API). */
wlt_update_error_t
wlt_update_verify_manifest(const char *manifest_path,
                          const char *manifest_sig_path,
                          const char *pubkey_path,
                          wlt_UpdateManifest *out)
{
    if (manifest_path == NULL || manifest_sig_path == NULL ||
        pubkey_path == NULL || out == NULL)
        return WLT_UPDATE_ERR_INVALID_ARG;

#ifndef HAVE_SODIUM
    WLT_UPDATE_LOG_ERR("manifest verify: built without libsodium");
    return WLT_UPDATE_ERR_UNSUPPORTED;
#else
    uint8_t pubkey[WLT_UPDATE_ED25519_PUBKEY_LEN];
    uint8_t sig[WLT_UPDATE_ED25519_SIG_LEN];
    wlt_update_error_t err = load_pubkey(pubkey_path, pubkey);
    if (err != WLT_UPDATE_OK) return err;
    err = load_signature(manifest_sig_path, sig);
    if (err != WLT_UPDATE_OK) return err;

    /* Read the whole manifest into memory (it's small). */
    FILE *fp = fopen(manifest_path, "rb");
    if (fp == NULL) return WLT_UPDATE_ERR_NOT_FOUND;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    /* Drain to detect truncation/oversize. */
    uint8_t extra;
    if (fread(&extra, 1, 1, fp) > 0) {
        fclose(fp);
        WLT_UPDATE_LOG_ERR("manifest too large (> %zu bytes)", sizeof(buf)-1);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }
    fclose(fp);
    buf[n] = '\0';

    if (sodium_init() < 0) return WLT_UPDATE_ERR_UNSUPPORTED;

    /* Verify the manifest signature. */
    if (crypto_sign_ed25519_verify_detached(sig, (unsigned char *)buf, n,
                                            pubkey) != 0) {
        WLT_UPDATE_LOG_ERR("manifest signature verification FAILED");
        return WLT_UPDATE_ERR_SIGNATURE;
    }

    /* Parse the manifest fields. */
    FILE *mp = fmemopen(buf, n, "r");
    if (mp == NULL) return WLT_UPDATE_ERR_IO;
    err = parse_manifest_file(mp, out);
    fclose(mp);
    if (err != WLT_UPDATE_OK) {
        WLT_UPDATE_LOG_ERR("manifest malformed");
        return err;
    }

    WLT_UPDATE_LOG_DBG("manifest OK: version=%s rollback=%llu slot=%c",
                       out->version, (unsigned long long)out->rollback_index,
                       out->target_slot);
    return WLT_UPDATE_OK;
#endif
}

/* Compute the SHA-256 of a file (streamed). Writes 64 lowercase hex
 * chars + NUL to out_hex. Used to verify the blob matches the manifest. */
wlt_update_error_t
wlt_update_compute_sha256(const char *path, char *out_hex, size_t out_hex_size)
{
    if (path == NULL || out_hex == NULL || out_hex_size < 65)
        return WLT_UPDATE_ERR_INVALID_ARG;

#ifndef HAVE_SODIUM
    WLT_UPDATE_LOG_ERR("sha256: built without libsodium");
    return WLT_UPDATE_ERR_UNSUPPORTED;
#else
    if (sodium_init() < 0) return WLT_UPDATE_ERR_UNSUPPORTED;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return WLT_UPDATE_ERR_NOT_FOUND;

    crypto_generichash_state st;
    if (crypto_generichash_init(&st, NULL, 0, crypto_generichash_BYTES) != 0) {
        fclose(fp);
        return WLT_UPDATE_ERR_IO;
    }

    uint8_t *buf = malloc(WLT_UPDATE_CHUNK_SIZE);
    if (buf == NULL) { fclose(fp); return WLT_UPDATE_ERR_NOMEM; }

    for (;;) {
        size_t n = fread(buf, 1, WLT_UPDATE_CHUNK_SIZE, fp);
        if (n == 0) {
            if (ferror(fp)) { fclose(fp); free(buf); return WLT_UPDATE_ERR_IO; }
            break;
        }
        if (crypto_generichash_update(&st, buf, n) != 0) {
            fclose(fp); free(buf); return WLT_UPDATE_ERR_IO;
        }
    }
    fclose(fp);
    free(buf);

    uint8_t hash[crypto_generichash_BYTES];  /* 32 bytes */
    if (crypto_generichash_final(&st, hash, sizeof(hash)) != 0)
        return WLT_UPDATE_ERR_IO;

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(hash); i++) {
        out_hex[i * 2]     = hex[(hash[i] >> 4) & 0xf];
        out_hex[i * 2 + 1] = hex[hash[i] & 0xf];
    }
    out_hex[64] = '\0';
    return WLT_UPDATE_OK;
#endif
}
