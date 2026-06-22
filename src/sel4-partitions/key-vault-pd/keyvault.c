/**
 * @file keyvault.c
 * @brief Main Key Vault PD — Microkit init/notified/protected, IPC handler
 * @module key-vault-pd
 * @purpose Handle encrypt/decrypt requests from Linux guest via seL4 IPC
 * @dependencies: keyvault.h, crypto.c, keystore.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md
 *
 * BARE METAL: No libc, no malloc, no filesystem.
 * All memory is statically allocated.
 * Keys are VOLATILE — lost on power off.
 */

#include "keyvault.h"
#include <string.h>

/* Microkit stubs (real implementation links against libmicrokit) */
#define microkit_channel int
#define microkit_msginfo uint64_t
#define MICROKIT_NOTIFY(ch) ((void)0)

/* Statically allocated shared memory region (mapped by Microkit) */
static uint8_t kv_shared_mem[KV_SHARED_MEM_SIZE];

/* PD state */
static bool kv_initialized = false;

void
init(void)
{
    /* Initialize key store (statically allocated, zeroed) */
    kv_keystore_init();
    kv_crypto_init();
    kv_initialized = true;
}

void
notified(microkit_channel ch)
{
    /* Called when another PD sends a notification */
    /* Read IPC header from shared memory */
    kv_ipc_header_t *hdr = (kv_ipc_header_t *)kv_shared_mem;
    void *data = kv_shared_mem + sizeof(kv_ipc_header_t);
    void *response = kv_shared_mem + sizeof(kv_ipc_header_t);

    /* Handle the request */
    size_t response_len = 0;
    hdr->status = kv_handle_request(hdr, data, response, &response_len);
    hdr->response_len = response_len;

    /* Notify the requester that the response is ready */
    MICROKIT_NOTIFY(ch);
}

void
protected(microkit_channel ch, microkit_msginfo msg)
{
    /* Protected procedure call — same as notified but with reply */
    notified(ch);
}

kv_error_t
kv_handle_request(kv_ipc_header_t *req, void *data,
                  void *response, size_t *response_len)
{
    if (req == NULL || !kv_initialized)
        return KV_ERR_INVALID;

    *response_len = 0;

    switch (req->op)
    {
        case KV_OP_PING:
            req->status = KV_OK;
            return KV_OK;

        case KV_OP_GENERATE_KEY:
            return kv_generate_key(req->key_id);

        case KV_OP_ENCRYPT:
            return kv_encrypt(req->key_id, (const uint8_t *)data, req->data_len,
                              (uint8_t *)response, response_len);

        case KV_OP_DECRYPT:
            return kv_decrypt(req->key_id, (const uint8_t *)data, req->data_len,
                              (uint8_t *)response, response_len);

        case KV_OP_DELETE_KEY:
            return kv_delete_key(req->key_id);

        case KV_OP_LIST_KEYS:
            return kv_list_keys((char *)response, response_len);

        default:
            return KV_ERR_INVALID;
    }
}
