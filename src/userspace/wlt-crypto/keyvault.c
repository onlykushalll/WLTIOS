/**
 * @file keyvault.c
 * @brief Key Vault IPC — STUB (seL4 PD communication not yet implemented)
 * @module wlt-crypto/keyvault
 * @purpose Placeholder for seL4 Key Vault PD communication
 * @dependencies: crypto.h, keyvault.h
 * @tested_by: tests/userspace/crypto_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * ARCHITECTURE NOTE (Claude's audit 2026-06-22):
 *
 * The previous implementation used Unix sockets (/run/key-vault.sock) to
 * communicate with the Key Vault PD. This is architecturally IMPOSSIBLE:
 * seL4 PDs are isolated from Linux and cannot expose Unix sockets.
 *
 * The correct IPC mechanism is seL4 Microkit channels (notification +
 * shared memory). Until the Microkit build is working, this module is a
 * STUB that returns WLT_CRYPTO_ERR_UNSUPPORTED.
 */

#include "crypto.h"
#include "keyvault.h"

#include <string.h>

wlt_crypto_error_t
wlt_keyvault_init(void)
{
    /* TODO: Implement seL4 Microkit channel setup */
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

void
wlt_keyvault_shutdown(void)
{
    /* TODO: Close seL4 notification channel */
}

wlt_crypto_error_t
wlt_keyvault_ping(void)
{
    /* TODO: Send ping via seL4 notification, wait for response */
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_encrypt(const char *key_id,
                     const uint8_t *plaintext, size_t plaintext_len,
                     uint8_t *ciphertext, size_t *ciphertext_len)
{
    (void)key_id; (void)plaintext; (void)plaintext_len;
    (void)ciphertext; (void)ciphertext_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_decrypt(const char *key_id,
                     const uint8_t *ciphertext, size_t ciphertext_len,
                     uint8_t *plaintext, size_t *plaintext_len)
{
    (void)key_id; (void)ciphertext; (void)ciphertext_len;
    (void)plaintext; (void)plaintext_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_generate_key(wlt_kv_key_type_t type,
                          char *key_id_out, size_t key_id_size)
{
    (void)type; (void)key_id_out; (void)key_id_size;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_delete_key(const char *key_id)
{
    (void)key_id;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_list_keys(char *buf, size_t buf_size, size_t *written)
{
    (void)buf; (void)buf_size;
    if (written) *written = 0;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}

wlt_crypto_error_t
wlt_keyvault_get_volume_key(const char *key_id,
                            uint8_t *key_out, size_t *key_len)
{
    (void)key_id; (void)key_out; (void)key_len;
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}
