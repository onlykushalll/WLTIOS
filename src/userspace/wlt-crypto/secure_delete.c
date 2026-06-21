/**
 * @file secure_delete.c
 * @brief Shred implementation: overwrite a file then unlink
 * @module wlt-crypto/secure_delete
 * @purpose Implements wlt_secure_delete_impl(), called by the public
 *          wlt_crypto_shred() dispatcher. Overwrites the file content
 *          multiple times (default 3: random, 0x00, 0xFF) plus a
 *          final zero pass, then truncates and unlinks. Refuses
 *          symlinks (would shred the target) and non-regular files.
 * @dependencies: crypto.h, libsodium (optional)
 * @tested_by: tests/userspace/secure_delete_test.c
 * @spec: docs/specs/wlt-crypto.md
 *
 * SECURITY NOTE: On modern flash and copy-on-write filesystems,
 * overwrite-based shredding is not a strong guarantee — the storage
 * device may remap blocks transparently. This implementation provides
 * best-effort shredding for HDD-backed filesystems. For full-disk
 * confidentiality, rely on LUKS (see the luks.c module).
 */

#include "crypto.h"

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

/* Buffer used for overwrite passes. 1 MiB balances memory use vs.
 * syscall overhead. */
#define WLT_SHRED_BUF_SIZE (1024 * 1024)

/* Pattern tags for the standard 3-pass shredding. The random pass uses
 * libsodium's CSPRNG when available, falling back to /dev/urandom. */
typedef enum {
    WLT_SHRED_PATTERN_RANDOM = 0,
    WLT_SHRED_PATTERN_ZEROS  = 1,
    WLT_SHRED_PATTERN_ONES   = 2,
} wlt_shred_pattern_t;

/* ─── Internal: Zeroing + Random Fill ────────────────── */

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

static wlt_crypto_error_t
fill_random(uint8_t *buf, size_t len)
{
#ifdef HAVE_SODIUM
    randombytes_buf(buf, len);
    return WLT_CRYPTO_OK;
#else
    /* Fallback: /dev/urandom. */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return WLT_CRYPTO_ERR_IO;
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = read(fd, buf + off, len - off);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            return WLT_CRYPTO_ERR_IO;
        }
        if (n == 0)
            break;
        off += (size_t)n;
    }
    close(fd);
    return (off == len) ? WLT_CRYPTO_OK : WLT_CRYPTO_ERR_IO;
#endif
}

static void
fill_pattern(uint8_t *buf, size_t len, wlt_shred_pattern_t pat)
{
    if (pat == WLT_SHRED_PATTERN_ZEROS)
        memset(buf, 0x00, len);
    else  /* WLT_SHRED_PATTERN_ONES */
        memset(buf, 0xFF, len);
}

/* Standard 3-pass: random, 0x00, 0xFF. For pass counts beyond 3 we
 * cycle through these patterns. */
static wlt_shred_pattern_t
pattern_for_pass(int pass_index)
{
    switch (pass_index % 3)
    {
        case 0:  return WLT_SHRED_PATTERN_RANDOM;
        case 1:  return WLT_SHRED_PATTERN_ZEROS;
        default: return WLT_SHRED_PATTERN_ONES;
    }
}

/* ─── Internal: Single Overwrite Pass ────────────────── */

static wlt_crypto_error_t
overwrite_pass(int fd, wlt_shred_pattern_t pat,
               uint8_t *buf, size_t buf_size)
{
    if (lseek(fd, 0, SEEK_SET) != 0 && errno != ESPIPE)
        return WLT_CRYPTO_ERR_IO;

    struct stat st;
    if (fstat(fd, &st) < 0)
        return WLT_CRYPTO_ERR_IO;

    uint64_t remaining = (uint64_t)st.st_size;
    while (remaining > 0)
    {
        size_t this_pass = (remaining < buf_size)
                         ? (size_t)remaining : buf_size;

        if (pat == WLT_SHRED_PATTERN_RANDOM)
        {
            wlt_crypto_error_t e = fill_random(buf, this_pass);
            if (e != WLT_CRYPTO_OK)
                return e;
        }
        else
        {
            fill_pattern(buf, this_pass, pat);
        }

        size_t off = 0;
        while (off < this_pass)
        {
            ssize_t w = write(fd, buf + off, this_pass - off);
            if (w < 0)
            {
                if (errno == EINTR || errno == EAGAIN)
                    continue;
                return WLT_CRYPTO_ERR_IO;
            }
            off += (size_t)w;
        }
        remaining -= this_pass;
    }

    /* Force the writes to disk before the next pass. */
    if (fsync(fd) < 0)
        WLT_CRYPTO_LOG_DBG("shred: fsync failed (continuing): %s",
                           strerror(errno));
    return WLT_CRYPTO_OK;
}

/* ─── Internal: Directory fsync ──────────────────────── */

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

/* ─── Public Implementation ──────────────────────────── */

wlt_crypto_error_t
wlt_secure_delete_impl(const char *path, int passes)
{
    if (path == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (passes <= 0)
        passes = WLT_CRYPTO_SHRED_DEFAULT_PASSES;
    if (passes > 32)
    {
        /* Cap at a sane upper bound — more passes waste time without
         * improving security on any modern storage device. */
        WLT_CRYPTO_LOG_WRN("shred: capping passes %d -> 32", passes);
        passes = 32;
    }

    /* Refuse symlinks — overwriting a symlink would shred its target,
     * which is almost certainly not what the caller intended. lstat
     * reveals the link itself rather than following it. */
    struct stat lst;
    if (lstat(path, &lst) < 0)
    {
        if (errno == ENOENT)
            return WLT_CRYPTO_ERR_NOT_FOUND;
        WLT_CRYPTO_LOG_ERR("shred: lstat(%s) failed: %s",
                           path, strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }
    if (S_ISLNK(lst.st_mode))
    {
        WLT_CRYPTO_LOG_ERR("shred: %s is a symlink (refusing)", path);
        return WLT_CRYPTO_ERR_INVALID_ARG;
    }
    if (!S_ISREG(lst.st_mode))
    {
        WLT_CRYPTO_LOG_ERR("shred: %s is not a regular file", path);
        return WLT_CRYPTO_ERR_INVALID_ARG;
    }

    int fd = open(path, O_WRONLY | O_SYNC);
    if (fd < 0)
    {
        if (errno == EACCES)
            return WLT_CRYPTO_ERR_PERMISSION;
        WLT_CRYPTO_LOG_ERR("shred: open(%s) failed: %s",
                           path, strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }

    uint8_t *buf = (uint8_t *)malloc(WLT_SHRED_BUF_SIZE);
    if (buf == NULL)
    {
        close(fd);
        return WLT_CRYPTO_ERR_NOMEM;
    }

    wlt_crypto_error_t err = WLT_CRYPTO_OK;
    for (int p = 0; p < passes; p++)
    {
        wlt_shred_pattern_t pat = pattern_for_pass(p);
        err = overwrite_pass(fd, pat, buf, WLT_SHRED_BUF_SIZE);
        if (err != WLT_CRYPTO_OK)
        {
            WLT_CRYPTO_LOG_ERR("shred: pass %d failed on %s: %s",
                               p, path, wlt_crypto_error_str(err));
            zero_buf(buf, WLT_SHRED_BUF_SIZE);
            free(buf);
            close(fd);
            return err;
        }
    }

    /* Final pass: zero everything so the file's pre-unlink content is
     * indistinguishable from a freshly-allocated empty file. */
    err = overwrite_pass(fd, WLT_SHRED_PATTERN_ZEROS,
                         buf, WLT_SHRED_BUF_SIZE);
    if (err != WLT_CRYPTO_OK)
        WLT_CRYPTO_LOG_WRN("shred: final zero pass failed (continuing)");

    zero_buf(buf, WLT_SHRED_BUF_SIZE);
    free(buf);

    /* Truncate to zero so the file's length is not visible post-unlink. */
    if (ftruncate(fd, 0) < 0)
        WLT_CRYPTO_LOG_DBG("shred: ftruncate failed: %s", strerror(errno));
    if (fsync(fd) < 0)
        WLT_CRYPTO_LOG_DBG("shred: final fsync failed: %s", strerror(errno));
    close(fd);

    if (unlink(path) < 0)
    {
        WLT_CRYPTO_LOG_ERR("shred: unlink(%s) failed: %s",
                           path, strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }

    fsync_parent_dir(path);
    WLT_CRYPTO_LOG_INF("shred: %s (%d passes)", path, passes);
    return WLT_CRYPTO_OK;
}
