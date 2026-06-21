/**
 * @file file_crypto.c
 * @brief Streaming per-file encryption with WLT1 framing
 * @module wlt-crypto/file_crypto
 * @purpose Chunk-by-chunk encrypt/decrypt via the Key Vault PD. Temp
 *          files + atomic rename so a crash never corrupts the original.
 *          File format documented in file_crypto.h.
 * @dependencies: crypto.h, keyvault.h, file_crypto.h, libsodium (optional)
 * @tested_by: tests/userspace/file_crypto_test.c
 * @spec: docs/specs/wlt-crypto.md
 */

#include "crypto.h"
#include "keyvault.h"
#include "file_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_SODIUM
#include <sodium.h>
#endif

static void
put_u32_le(uint8_t buf[4], uint32_t v)
{
    buf[0] = (uint8_t)(v & 0xff);
    buf[1] = (uint8_t)((v >> 8) & 0xff);
    buf[2] = (uint8_t)((v >> 16) & 0xff);
    buf[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint32_t
get_u32_le(const uint8_t buf[4])
{
    return  (uint32_t)buf[0]
          | ((uint32_t)buf[1] << 8)
          | ((uint32_t)buf[2] << 16)
          | ((uint32_t)buf[3] << 24);
}

static void
put_u64_le(uint8_t buf[8], uint64_t v)
{
    for (int i = 0; i < 8; i++)
        buf[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}

static uint64_t
get_u64_le(const uint8_t buf[8])
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= ((uint64_t)buf[i]) << (8 * i);
    return v;
}

static void
zero_buf(void *buf, size_t len)
{
    if (buf == NULL || len == 0)
        return;
#ifdef HAVE_SODIUM
    sodium_memzero(buf, len);
#else
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--)
        *p++ = 0;
#endif
}

/* io_exact: transfer exactly n bytes, retry on EINTR; 0 or -1. */

static int
io_exact(int fd, void *buf, size_t n, bool is_write)
{
    uint8_t *p = (uint8_t *)buf;
    size_t off = 0;
    while (off < n)
    {
        ssize_t r = is_write ? write(fd, p + off, n - off)
                             : read(fd, p + off, n - off);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (!is_write && r == 0)
            return -1;
        off += (size_t)r;
    }
    return 0;
}

static int
read_exact(int fd, void *buf, size_t n)
{
    return io_exact(fd, buf, n, false);
}

static int
write_exact(int fd, const void *buf, size_t n)
{
    return io_exact(fd, (void *)buf, n, true);
}

static void
fsync_parent_dir(const char *path)
{
    char dir[WLT_CRYPTO_MAX_PATH];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash == NULL)
        return;
    if (slash == dir)
        dir[1] = '\0';
    else
        *slash = '\0';
    int dfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dfd < 0)
        return;
    fsync(dfd);
    close(dfd);
}

static void
release_files(int in_fd, int out_fd, const char *tmp, bool unlink_tmp)
{
    if (in_fd >= 0)
        close(in_fd);
    if (out_fd >= 0)
        close(out_fd);
    if (unlink_tmp && tmp != NULL)
        unlink(tmp);
}

static wlt_crypto_error_t
open_input_file(const char *path, int *fd_out, uint64_t *len_out)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: open(%s) failed: %s",
                           path, strerror(errno));
        return (errno == ENOENT) ? WLT_CRYPTO_ERR_NOT_FOUND
                                 : WLT_CRYPTO_ERR_IO;
    }
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return WLT_CRYPTO_ERR_IO;
    }
    *fd_out = fd;
    if (len_out != NULL)
        *len_out = (uint64_t)st.st_size;
    return WLT_CRYPTO_OK;
}

static wlt_crypto_error_t
open_output_tmp(const char *path, const char *suffix,
                char *tmp, size_t tmp_size, int *out_fd)
{
    size_t plen = strlen(path);
    size_t slen = strlen(suffix);
    if (plen + slen + 1 > tmp_size)
        return WLT_CRYPTO_ERR_TOO_LARGE;
    memcpy(tmp, path, plen);
    memcpy(tmp + plen, suffix, slen + 1);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: open(%s) failed: %s",
                           tmp, strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }
    *out_fd = fd;
    return WLT_CRYPTO_OK;
}

/* fsync + close both fds, atomically rename tmp over path, fsync parent. */
static wlt_crypto_error_t
finalize_replace(int in_fd, int out_fd, const char *tmp, const char *path)
{
    if (fsync(out_fd) < 0)
        WLT_CRYPTO_LOG_DBG("file_crypto: fsync(out) failed: %s",
                           strerror(errno));
    close(out_fd);
    close(in_fd);
    if (rename(tmp, path) < 0)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: rename failed: %s", strerror(errno));
        unlink(tmp);
        return WLT_CRYPTO_ERR_IO;
    }
    fsync_parent_dir(path);
    return WLT_CRYPTO_OK;
}

static wlt_crypto_error_t
stream_encrypt(int in_fd, int out_fd, const char *key_id,
               size_t chunk, uint64_t *total_out)
{
    uint8_t *plain  = (uint8_t *)malloc(chunk);
    uint8_t *cipher = (uint8_t *)malloc(chunk + WLT_CRYPTO_CIPHERTEXT_OVERHEAD);
    if (plain == NULL || cipher == NULL)
    {
        free(plain);
        free(cipher);
        return WLT_CRYPTO_ERR_NOMEM;
    }

    wlt_crypto_error_t err = WLT_CRYPTO_OK;
    uint64_t total = 0;
    for (;;)
    {
        ssize_t r = read(in_fd, plain, chunk);
        if (r < 0)
        {
            if (errno == EINTR)
                continue;
            err = WLT_CRYPTO_ERR_IO;
            break;
        }
        if (r == 0)
            break;

        size_t clen = chunk + WLT_CRYPTO_CIPHERTEXT_OVERHEAD;
        err = wlt_keyvault_encrypt(key_id, plain, (size_t)r,
                                   cipher, &clen);
        if (err != WLT_CRYPTO_OK)
        {
            zero_buf(plain, (size_t)r);
            break;
        }

        uint8_t lb[4];
        put_u32_le(lb, (uint32_t)clen);
        if (write_exact(out_fd, lb, 4) != 0 ||
            write_exact(out_fd, cipher, clen) != 0)
        {
            err = WLT_CRYPTO_ERR_IO;
            zero_buf(plain, (size_t)r);
            break;
        }
        zero_buf(plain, (size_t)r);
        total += (uint64_t)r;
    }

    zero_buf(plain, chunk);
    zero_buf(cipher, chunk + WLT_CRYPTO_CIPHERTEXT_OVERHEAD);
    free(plain);
    free(cipher);
    *total_out = total;
    return err;
}

wlt_crypto_error_t
wlt_file_crypto_encrypt_file(const char *path, const char *key_id)
{
    if (path == NULL || key_id == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    int in_fd = -1, out_fd = -1;
    char tmp[WLT_CRYPTO_MAX_PATH];
    uint64_t file_len = 0;
    const size_t chunk = WLT_CRYPTO_DEFAULT_CHUNK_SIZE;

    wlt_crypto_error_t err = open_input_file(path, &in_fd, &file_len);
    if (err != WLT_CRYPTO_OK)
        return err;
    err = open_output_tmp(path, ".wltenc.tmp", tmp, sizeof(tmp), &out_fd);
    if (err != WLT_CRYPTO_OK)
    {
        close(in_fd);
        return err;
    }

    /* Build + write the WLT1 header. */
    uint8_t hdr[WLT_FILE_CRYPTO_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr + WLT_FILE_CRYPTO_OFF_MAGIC,
           WLT_FILE_CRYPTO_MAGIC, WLT_FILE_CRYPTO_MAGIC_LEN);
    hdr[WLT_FILE_CRYPTO_OFF_VERSION] = WLT_FILE_CRYPTO_VERSION;
    hdr[WLT_FILE_CRYPTO_OFF_FLAGS]   = WLT_FILE_CRYPTO_FLAG_HAS_LENGTH;
    size_t klen = strlen(key_id);
    if (klen > WLT_CRYPTO_MAX_KEY_ID)
        klen = WLT_CRYPTO_MAX_KEY_ID;
    memcpy(hdr + WLT_FILE_CRYPTO_OFF_KEY_ID, key_id, klen);
    put_u32_le(hdr + WLT_FILE_CRYPTO_OFF_KEY_ID_LEN, (uint32_t)klen);
    put_u32_le(hdr + WLT_FILE_CRYPTO_OFF_CHUNK_SIZE, (uint32_t)chunk);
    put_u64_le(hdr + WLT_FILE_CRYPTO_OFF_FILE_LEN, file_len);
    if (write_exact(out_fd, hdr, sizeof(hdr)) != 0)
    {
        release_files(in_fd, out_fd, tmp, true);
        return WLT_CRYPTO_ERR_IO;
    }

    uint64_t total = 0;
    err = stream_encrypt(in_fd, out_fd, key_id, chunk, &total);
    if (err != WLT_CRYPTO_OK)
    {
        release_files(in_fd, out_fd, tmp, true);
        return err;
    }

    /* End marker: 4-byte LE zero after the final chunk. */
    uint8_t end[4] = {0, 0, 0, 0};
    if (write_exact(out_fd, end, 4) != 0)
    {
        release_files(in_fd, out_fd, tmp, true);
        return WLT_CRYPTO_ERR_IO;
    }

    err = finalize_replace(in_fd, out_fd, tmp, path);
    if (err == WLT_CRYPTO_OK)
        WLT_CRYPTO_LOG_DBG("file_crypto: encrypted %s (%llu bytes)",
                           path, (unsigned long long)total);
    return err;
}

static wlt_crypto_error_t
stream_decrypt(int in_fd, int out_fd, const char *key_id,
               uint32_t chunk_size, uint64_t *total_out)
{
    uint8_t *cipher = (uint8_t *)malloc(chunk_size + WLT_CRYPTO_CIPHERTEXT_OVERHEAD);
    uint8_t *plain  = (uint8_t *)malloc(chunk_size);
    if (cipher == NULL || plain == NULL)
    {
        free(cipher);
        free(plain);
        return WLT_CRYPTO_ERR_NOMEM;
    }

    wlt_crypto_error_t err = WLT_CRYPTO_OK;
    uint64_t total = 0;
    for (;;)
    {
        uint8_t lb[4];
        if (read_exact(in_fd, lb, 4) != 0)
        {
            WLT_CRYPTO_LOG_ERR("file_crypto: truncated length prefix");
            err = WLT_CRYPTO_ERR_FORMAT;
            break;
        }
        uint32_t clen = get_u32_le(lb);
        if (clen == WLT_FILE_CRYPTO_END_MARKER)
            break;
        if (clen > chunk_size + WLT_CRYPTO_CIPHERTEXT_OVERHEAD)
        {
            WLT_CRYPTO_LOG_ERR("file_crypto: chunk too large (%u)", clen);
            err = WLT_CRYPTO_ERR_FORMAT;
            break;
        }
        if (read_exact(in_fd, cipher, clen) != 0)
        {
            err = WLT_CRYPTO_ERR_FORMAT;
            break;
        }

        size_t plain_len = chunk_size;
        err = wlt_keyvault_decrypt(key_id, cipher, clen,
                                   plain, &plain_len);
        if (err != WLT_CRYPTO_OK)
        {
            WLT_CRYPTO_LOG_ERR("file_crypto: decrypt failed "
                               "(possible attack)");
            break;
        }

        if (write_exact(out_fd, plain, plain_len) != 0)
        {
            err = WLT_CRYPTO_ERR_IO;
            zero_buf(plain, plain_len);
            break;
        }
        zero_buf(plain, plain_len);
        total += plain_len;
    }

    zero_buf(cipher, chunk_size + WLT_CRYPTO_CIPHERTEXT_OVERHEAD);
    zero_buf(plain, chunk_size);
    free(cipher);
    free(plain);
    *total_out = total;
    return err;
}

wlt_crypto_error_t
wlt_file_crypto_decrypt_file(const char *path, const char *key_id)
{
    if (path == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    int in_fd = -1, out_fd = -1;
    char tmp[WLT_CRYPTO_MAX_PATH];
    char hdr_key_id[WLT_CRYPTO_MAX_KEY_ID + 1];
    uint32_t chunk_size = 0;
    uint64_t expected_len = 0;
    uint8_t flags = 0;

    wlt_crypto_error_t err = open_input_file(path, &in_fd, NULL);
    if (err != WLT_CRYPTO_OK)
        return err;

    /* Read + validate the WLT1 header. */
    uint8_t hdr[WLT_FILE_CRYPTO_HEADER_SIZE];
    if (read_exact(in_fd, hdr, sizeof(hdr)) != 0)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: short header on %s", path);
        close(in_fd);
        return WLT_CRYPTO_ERR_FORMAT;
    }
    if (memcmp(hdr + WLT_FILE_CRYPTO_OFF_MAGIC,
               WLT_FILE_CRYPTO_MAGIC,
               WLT_FILE_CRYPTO_MAGIC_LEN) != 0)
    {
        close(in_fd);
        return WLT_CRYPTO_ERR_FORMAT;
    }
    if (hdr[WLT_FILE_CRYPTO_OFF_VERSION] != WLT_FILE_CRYPTO_VERSION)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: unsupported version %u",
                           hdr[WLT_FILE_CRYPTO_OFF_VERSION]);
        close(in_fd);
        return WLT_CRYPTO_ERR_FORMAT;
    }
    uint32_t klen = get_u32_le(hdr + WLT_FILE_CRYPTO_OFF_KEY_ID_LEN);
    if (klen >= sizeof(hdr_key_id) || klen > WLT_CRYPTO_MAX_KEY_ID)
    {
        close(in_fd);
        return WLT_CRYPTO_ERR_FORMAT;
    }
    memcpy(hdr_key_id, hdr + WLT_FILE_CRYPTO_OFF_KEY_ID, klen);
    hdr_key_id[klen] = '\0';
    chunk_size  = get_u32_le(hdr + WLT_FILE_CRYPTO_OFF_CHUNK_SIZE);
    expected_len = get_u64_le(hdr + WLT_FILE_CRYPTO_OFF_FILE_LEN);
    flags = hdr[WLT_FILE_CRYPTO_OFF_FLAGS];

    const char *use_key_id = key_id;
    if (use_key_id == NULL)
        use_key_id = hdr_key_id;
    else if (strcmp(use_key_id, hdr_key_id) != 0)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: key ID mismatch "
                           "(caller=%s, hdr=%s)", use_key_id, hdr_key_id);
        close(in_fd);
        return WLT_CRYPTO_ERR_KEY_NOT_FOUND;
    }

    if (chunk_size == 0 || chunk_size > 16 * 1024 * 1024)
    {
        close(in_fd);
        return WLT_CRYPTO_ERR_FORMAT;
    }

    err = open_output_tmp(path, ".wltdec.tmp", tmp, sizeof(tmp), &out_fd);
    if (err != WLT_CRYPTO_OK)
    {
        close(in_fd);
        return err;
    }

    uint64_t total = 0;
    err = stream_decrypt(in_fd, out_fd, use_key_id, chunk_size, &total);
    if (err != WLT_CRYPTO_OK)
    {
        release_files(in_fd, out_fd, tmp, true);
        return err;
    }

    if ((flags & WLT_FILE_CRYPTO_FLAG_HAS_LENGTH) &&
        total != expected_len)
    {
        WLT_CRYPTO_LOG_ERR("file_crypto: length mismatch on %s "
                           "(got %llu, expected %llu)", path,
                           (unsigned long long)total,
                           (unsigned long long)expected_len);
        release_files(in_fd, out_fd, tmp, true);
        return WLT_CRYPTO_ERR_FORMAT;
    }

    err = finalize_replace(in_fd, out_fd, tmp, path);
    if (err == WLT_CRYPTO_OK)
        WLT_CRYPTO_LOG_DBG("file_crypto: decrypted %s (%llu bytes)",
                           path, (unsigned long long)total);
    return err;
}
