/**
 * @file keyring.c
 * @brief Key chain + rotation implementation (research-I R7)
 */
#include "keyring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#define WLT_KEYRING_MAGIC 0x574c544b  /* "WLTK" */
#define WLT_KEYRING_PATH  "/etc/wlt/keys/update-keyring.bin"

/* Read exactly n bytes from fd; returns 0 on success, -1 on error/EOF. */
static int read_exact(int fd, void *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, (char *)buf + off, n - off);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) return -1;  /* unexpected EOF */
        off += (size_t)r;
    }
    return 0;
}

/* Read a little-endian u32. */
static int read_u32(int fd, uint32_t *out) {
    uint8_t b[4];
    if (read_exact(fd, b, 4) < 0) return -1;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

/* Read a little-endian u64. */
static int read_u64(int fd, uint64_t *out) {
    uint8_t b[8];
    if (read_exact(fd, b, 8) < 0) return -1;
    *out = 0;
    for (int i = 0; i < 8; i++)
        *out |= (uint64_t)b[i] << (i * 8);
    return 0;
}

wlt_update_error_t
wlt_keyring_load(wlt_Keyring *kr)
{
    if (kr == NULL)
        return WLT_UPDATE_ERR_INVALID_ARG;
    memset(kr, 0, sizeof(*kr));

    int fd = open(WLT_KEYRING_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT)
            return WLT_UPDATE_ERR_NOT_FOUND;
        return WLT_UPDATE_ERR_IO;
    }

    uint32_t magic = 0;
    if (read_u32(fd, &magic) < 0 || magic != WLT_KEYRING_MAGIC) {
        close(fd);
        WLT_UPDATE_LOG_ERR("keyring: bad magic (0x%08x)", magic);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

    uint32_t count = 0;
    if (read_u32(fd, &count) < 0) {
        close(fd);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }
    if (count > WLT_KEYRING_MAX_ENTRIES) {
        WLT_UPDATE_LOG_ERR("keyring: %u entries exceeds max %d",
                           count, WLT_KEYRING_MAX_ENTRIES);
        close(fd);
        return WLT_UPDATE_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < count; i++) {
        wlt_KeyringEntry *e = &kr->entries[i];
        uint8_t id_len = 0;
        if (read_exact(fd, &id_len, 1) < 0 || id_len > WLT_KEYRING_MAX_KEY_ID) {
            close(fd);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        char id_buf[WLT_KEYRING_MAX_KEY_ID + 1] = {0};
        if (read_exact(fd, id_buf, WLT_KEYRING_MAX_KEY_ID) < 0) {
            close(fd);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        /* Copy exactly id_len chars, NUL-terminate. */
        memcpy(e->key_id, id_buf, id_len);
        e->key_id[id_len] = '\0';

        if (read_exact(fd, e->pubkey, WLT_UPDATE_ED25519_PUBKEY_LEN) < 0) {
            close(fd);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        if (read_u64(fd, &e->not_before) < 0 ||
            read_u64(fd, &e->not_after) < 0) {
            close(fd);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        e->loaded = true;
    }
    close(fd);

    kr->count = count;
    kr->loaded = true;
    WLT_UPDATE_LOG_INF("keyring: loaded %zu entries from %s",
                       kr->count, WLT_KEYRING_PATH);
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_keyring_lookup(const wlt_Keyring *kr, const char *key_id,
                   uint64_t now_epoch, uint8_t *out_pubkey)
{
    if (kr == NULL || key_id == NULL || out_pubkey == NULL)
        return WLT_UPDATE_ERR_INVALID_ARG;
    if (!kr->loaded)
        return WLT_UPDATE_ERR_NOT_FOUND;

    for (size_t i = 0; i < kr->count; i++) {
        const wlt_KeyringEntry *e = &kr->entries[i];
        if (!e->loaded)
            continue;
        if (strcmp(e->key_id, key_id) != 0)
            continue;
        /* Found the key_id; check the validity window. */
        if (e->not_before != 0 && now_epoch < e->not_before) {
            WLT_UPDATE_LOG_ERR("keyring: key %s not yet valid (now=%llu before=%llu)",
                               key_id, (unsigned long long)now_epoch,
                               (unsigned long long)e->not_before);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        if (e->not_after != 0 && now_epoch > e->not_after) {
            WLT_UPDATE_LOG_ERR("keyring: key %s expired (now=%llu after=%llu)",
                               key_id, (unsigned long long)now_epoch,
                               (unsigned long long)e->not_after);
            return WLT_UPDATE_ERR_INVALID_ARG;
        }
        memcpy(out_pubkey, e->pubkey, WLT_UPDATE_ED25519_PUBKEY_LEN);
        return WLT_UPDATE_OK;
    }
    WLT_UPDATE_LOG_ERR("keyring: key_id '%s' not found", key_id);
    return WLT_UPDATE_ERR_NOT_FOUND;
}

wlt_update_error_t
wlt_keyring_find(const char *key_id, uint8_t *out_pubkey)
{
    wlt_Keyring kr;
    wlt_update_error_t err = wlt_keyring_load(&kr);
    if (err != WLT_UPDATE_OK)
        return err;
    return wlt_keyring_lookup(&kr, key_id, (uint64_t)time(NULL), out_pubkey);
}
