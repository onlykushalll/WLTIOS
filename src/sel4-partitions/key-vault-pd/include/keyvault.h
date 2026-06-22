/**
 * @file keyvault.h
 * @brief Public interface for Key Vault seL4 Protection Domain
 * @module key-vault-pd
 * @purpose Encrypt/decrypt data with keys that never leave the PD
 * @dependencies: seL4 Microkit
 * @tested_by: tests/sel4/keyvault_test.c
 * @spec: docs/specs/key-vault-pd.md
 * @adr: docs/adr/02-sel4-as-base.md, docs/adr/04-microkit-libvmm.md
 *
 * ARCHITECTURE: This is a bare-metal seL4 Protection Domain.
 * NO libc, NO filesystem, NO sockets, NO malloc.
 * Keys are stored in VOLATILE private memory (lost on power off).
 * Persistence across reboots requires encrypted disk storage
 * loaded at boot after user authentication (design TBD).
 */

#ifndef WLT_KEYVAULT_PD_H
#define WLT_KEYVAULT_PD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KV_MAX_KEYS 32
#define KV_MAX_KEY_ID 64
#define KV_MAX_KEY_SIZE 64
#define KV_MAX_DATA_SIZE 65536
#define KV_SHARED_MEM_SIZE (KV_MAX_DATA_SIZE + 256)

typedef enum {
    KV_OK = 0,
    KV_ERR_INVALID = -1,
    KV_ERR_NOT_FOUND = -2,
    KV_ERR_EXISTS = -3,
    KV_ERR_FULL = -4,
    KV_ERR_TOO_LARGE = -5,
    KV_ERR_AUTH = -6,
    KV_ERR_INTERNAL = -7,
} kv_error_t;

typedef enum {
    KV_OP_PING = 0,
    KV_OP_GENERATE_KEY = 1,
    KV_OP_ENCRYPT = 2,
    KV_OP_DECRYPT = 3,
    KV_OP_DELETE_KEY = 4,
    KV_OP_LIST_KEYS = 5,
    KV_OP_GET_KEY_INFO = 6,
} kv_op_t;

typedef struct {
    char key_id[KV_MAX_KEY_ID];
    uint8_t key_data[KV_MAX_KEY_SIZE];
    size_t key_len;
    bool in_use;
} kv_key_entry_t;

typedef struct {
    kv_op_t op;
    int32_t status;
    char key_id[KV_MAX_KEY_ID];
    size_t data_len;
    size_t response_len;
    /* Data follows in shared memory after this header */
} kv_ipc_header_t;

/* PD entry points (called by Microkit) */
void init(void);
void notified(microkit_channel ch);
void protected(microkit_channel ch, microkit_msginfo msg);

/* Internal API */
kv_error_t kv_handle_request(kv_ipc_header_t *req, void *data,
                              void *response, size_t *response_len);
kv_error_t kv_generate_key(const char *key_id);
kv_error_t kv_encrypt(const char *key_id, const uint8_t *plaintext, size_t plen,
                      uint8_t *ciphertext, size_t *clen);
kv_error_t kv_decrypt(const char *key_id, const uint8_t *ciphertext, size_t clen,
                      uint8_t *plaintext, size_t *plen);
kv_error_t kv_delete_key(const char *key_id);
kv_error_t kv_list_keys(char *key_ids, size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* WLT_KEYVAULT_PD_H */
