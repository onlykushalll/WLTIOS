/**
 * @file keyvault.c
 * @brief Key Vault IPC — STUB (seL4 PD communication not yet implemented)
 * @module wlt-crypto/keyvault
 * @purpose Placeholder for seL4 Key Vault PD communication
 * @dependencies: crypto.h, keyvault.h
 * @tested_by: tests/userspace/keyvault_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * ARCHITECTURE NOTE (Claude's audit 2026-06-22):
 *
 * The previous implementation used Unix sockets (/run/key-vault.sock) to
 * communicate with the Key Vault PD. This is architecturally IMPOSSIBLE:
 *
 *   - seL4 Protection Domains are isolated from Linux
 *   - PDs have no libc, no filesystem, no network stack
 *   - PDs cannot open or listen on Unix sockets
 *   - Unix sockets are a Linux construct, not available in seL4
 *
 * The correct IPC mechanism is:
 *   - seL4 Microkit channels (notification + shared memory)
 *   - Or seL4 message-passing (seL4_Call / seL4_Recv with caps)
 *   - Shared memory regions mapped at boot, with notification signals
 *
 * Until the seL4 Microkit build is working (currently using CAmkES),
 * this module is a STUB that returns WLT_CRYPTO_ERR_NOT_IMPLEMENTED.
 *
 * See: docs/context/RESEARCH_SEL4_PARTITIONS.md section
 *      "How does the Key Vault encrypt/decrypt files for Linux apps?"
 */

#include "crypto.h"
#include "keyvault.h"

#include <string.h>

wlt_crypto_error_t
wlt_keyvault_init(void)
{
    /* TODO(wlt-crypto): Implement seL4 Microkit channel setup */
    /* When the Microkit build is working, this will: */
    /*   1. Initialize the seL4 notification channel */
    /*   2. Map shared memory region for data transfer */
    /*   3. Verify Key Vault PD is alive (ping via notification) */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_encrypt(const char *key_id,
                      const uint8_t *plaintext, size_t plaintext_len,
                      uint8_t *ciphertext, size_t *ciphertext_len)
{
    (void)key_id;
    (void)plaintext;
    (void)plaintext_len;
    (void)ciphertext;
    (void)ciphertext_len;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    /* When implemented, this will: */
    /*   1. Write plaintext to shared memory region */
    /*   2. Send encrypt request via seL4 notification */
    /*   3. Wait for Key Vault PD response */
    /*   4. Read ciphertext from shared memory */
    /*   5. Zero the shared memory buffer */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_decrypt(const char *key_id,
                      const uint8_t *ciphertext, size_t ciphertext_len,
                      uint8_t *plaintext, size_t *plaintext_len)
{
    (void)key_id;
    (void)ciphertext;
    (void)ciphertext_len;
    (void)plaintext;
    (void)plaintext_len;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_generate_key(const char *key_id)
{
    (void)key_id;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_delete_key(const char *key_id)
{
    (void)key_id;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_list_keys(char *key_ids, size_t *count)
{
    (void)key_ids;
    (void)count;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_get_volume_key(const char *key_id,
                             uint8_t *key, size_t *key_len)
{
    (void)key_id;
    (void)key;
    (void)key_len;

    /* TODO(wlt-crypto): Implement via seL4 IPC */
    /* This is the one operation that returns raw key material */
    /* (needed for dm-crypt ioctl). Key is zeroed by caller on failure. */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}

wlt_crypto_error_t
wlt_keyvault_shutdown(void)
{
    /* TODO(wlt-crypto): Clean up seL4 channels */
    return WLT_CRYPTO_ERR_NOT_IMPLEMENTED;
}
