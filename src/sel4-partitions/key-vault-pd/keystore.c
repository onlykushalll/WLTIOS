/**
 * @file keystore.c
 * @brief Key storage in private volatile memory
 * @module key-vault-pd/keystore
 * @purpose Store encryption keys in PD private memory (not accessible to Linux)
 * @dependencies: keyvault.h, crypto.c
 * @spec: docs/specs/key-vault-pd.md
 *
 * Keys are VOLATILE — lost on power off.
 * Persistence requires encrypted disk storage (design TBD).
 */

#include "keyvault.h"
#include <string.h>

static kv_key_entry_t g_keys[KV_MAX_KEYS];

void
kv_keystore_init(void)
{
    memset(g_keys, 0, sizeof(g_keys));
}

static kv_key_entry_t *
find_key(const char *key_id)
{
    for (int i = 0; i < KV_MAX_KEYS; i++)
    {
        if (g_keys[i].in_use && strcmp(g_keys[i].key_id, key_id) == 0)
            return &g_keys[i];
    }
    return NULL;
}

static kv_key_entry_t *
find_free_slot(void)
{
    for (int i = 0; i < KV_MAX_KEYS; i++)
    {
        if (!g_keys[i].in_use)
            return &g_keys[i];
    }
    return NULL;
}

kv_error_t
kv_generate_key(const char *key_id)
{
    if (key_id == NULL || key_id[0] == '\0')
        return KV_ERR_INVALID;

    if (find_key(key_id) != NULL)
        return KV_ERR_EXISTS;

    kv_key_entry_t *slot = find_free_slot();
    if (slot == NULL)
        return KV_ERR_FULL;

    strncpy(slot->key_id, key_id, KV_MAX_KEY_ID - 1);
    slot->key_id[KV_MAX_KEY_ID - 1] = '\0';

    /* Generate key: use key_id as seed (TODO: real RNG) */
    kv_crypto_generate_key((const uint8_t *)key_id, strlen(key_id),
                           slot->key_data, 32);
    slot->key_len = 32;
    slot->in_use = true;

    return KV_OK;
}

kv_error_t
kv_encrypt(const char *key_id, const uint8_t *plaintext, size_t plen,
           uint8_t *ciphertext, size_t *clen)
{
    if (key_id == NULL || plaintext == NULL || ciphertext == NULL || clen == NULL)
        return KV_ERR_INVALID;

    kv_key_entry_t *key = find_key(key_id);
    if (key == NULL)
        return KV_ERR_NOT_FOUND;

    return kv_crypto_encrypt(key->key_data, key->key_len,
                              plaintext, plen, ciphertext, clen);
}

kv_error_t
kv_decrypt(const char *key_id, const uint8_t *ciphertext, size_t clen,
           uint8_t *plaintext, size_t *plen)
{
    if (key_id == NULL || ciphertext == NULL || plaintext == NULL || plen == NULL)
        return KV_ERR_INVALID;

    kv_key_entry_t *key = find_key(key_id);
    if (key == NULL)
        return KV_ERR_NOT_FOUND;

    return kv_crypto_decrypt(key->key_data, key->key_len,
                              ciphertext, clen, plaintext, plen);
}

kv_error_t
kv_delete_key(const char *key_id)
{
    if (key_id == NULL)
        return KV_ERR_INVALID;

    kv_key_entry_t *key = find_key(key_id);
    if (key == NULL)
        return KV_ERR_NOT_FOUND;

    /* Zero the key material before clearing */
    memset(key->key_data, 0, KV_MAX_KEY_SIZE);
    key->key_len = 0;
    key->in_use = false;
    key->key_id[0] = '\0';

    return KV_OK;
}

kv_error_t
kv_list_keys(char *key_ids, size_t *count)
{
    if (key_ids == NULL || count == NULL)
        return KV_ERR_INVALID;

    *count = 0;
    char *ptr = key_ids;

    for (int i = 0; i < KV_MAX_KEYS; i++)
    {
        if (g_keys[i].in_use)
        {
            size_t id_len = strlen(g_keys[i].key_id);
            strncpy(ptr, g_keys[i].key_id, id_len);
            ptr[id_len] = '\n';
            ptr += id_len + 1;
            (*count)++;
        }
    }
    *ptr = '\0';

    return KV_OK;
}
