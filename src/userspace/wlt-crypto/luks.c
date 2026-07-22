/*
 * luks.c — LUKS2 disk encryption implementation
 *
 * Uses cryptsetup as an external tool (same pattern as partition.c
 * uses mkfs.ext4/mount). This avoids the complexity of implementing
 * LUKS2 from scratch while providing real disk encryption.
 *
 * Key source: the LUKS volume key is supplied by the caller via an
 * open file descriptor (key_fd). The _impl functions read from key_fd
 * and materialise the bytes into a per-call unique keyfile (mkstemp +
 * O_NOFOLLOW + 0600) which is then passed to cryptsetup via
 * --key-file=<path>. After cryptsetup completes the keyfile is
 * zeroised, fsynced, and unlinked. There is NO hardcoded passphrase
 * fallback — if key_fd < 0 the call fails with
 * WLT_CRYPTO_ERR_INVALID_ARG. The Key Vault PD integration that
 * supplies key_fd is tracked as a separate architectural task (see
 * research-H in worklog).
 */

#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>

extern char **environ;

#define CRYPTSETUP_PATH "/usr/sbin/cryptsetup"
#define LUKS_KEYFILE_DIR "/run/wlt"
#define LUKS_KEYFILE_TEMPLATE LUKS_KEYFILE_DIR "/luks-keyfile.XXXXXX"
/* Bytes of 0x00 written over the keyfile before unlink. 128 covers the
 * typical volume-key/passphrase length; the file is also ftruncated to 0. */
#define LUKS_KEYFILE_ZEROS 128

static void
zero_buf(void *buf, size_t len)
{
    if (buf == NULL || len == 0)
        return;
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--)
        *p++ = 0;
}

static wlt_crypto_error_t
run_cmd(const char *file, char *const argv[], int *exit_code)
{
    if (file == NULL || argv == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    pid_t pid = fork();
    if (pid < 0) {
        return WLT_CRYPTO_ERR_IO;
    }
    if (pid == 0) {
        execve(file, argv, environ);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return WLT_CRYPTO_ERR_IO;
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (exit_code)
        *exit_code = code;
    if (!WIFEXITED(status) || code != 0)
        return WLT_CRYPTO_ERR_IO;
    return WLT_CRYPTO_OK;
}

/* write_keyfile: read key bytes from key_fd and materialise them in a
 * per-call unique keyfile under /run/wlt. The path is written to
 * `out_path` (capacity out_path_size). The caller MUST call
 * remove_keyfile(out_path) to zeroise + unlink the keyfile.
 *
 * Security properties vs. the previous implementation:
 *  - mkstemp creates the file with O_EXCL + 0600 (no symlink-swap race,
 *    no world-readable window, no concurrent-call collision on a fixed
 *    path). Closes D-3 / A-H1.
 *  - fchmod(0600) + fcntl(FD_CLOEXEC) harden against a hostile umask
 *    and fd leak into the cryptsetup child.
 *  - The key contents come from key_fd (caller-supplied) rather than a
 *    compile-time constant. Closes D-1.
 */
static wlt_crypto_error_t
write_keyfile(int key_fd, char *out_path, size_t out_path_size)
{
    if (out_path == NULL || out_path_size < sizeof(LUKS_KEYFILE_TEMPLATE))
        return WLT_CRYPTO_ERR_INVALID_ARG;

    /* Best-effort: ensure /run/wlt exists. mkdir errors other than EEXIST
     * are caught by the subsequent mkstemp failure. */
    (void)mkdir(LUKS_KEYFILE_DIR, 0700);

    if (snprintf(out_path, out_path_size, "%s", LUKS_KEYFILE_TEMPLATE)
            >= (int)out_path_size)
        return WLT_CRYPTO_ERR_TOO_LARGE;

    /* mkstemp mutates the trailing XXXXXX in out_path and returns an fd
     * opened O_RDWR | O_CREAT | O_EXCL with mode 0600. */
    int kfd = mkstemp(out_path);
    if (kfd < 0) {
        WLT_CRYPTO_LOG_ERR("luks: mkstemp failed: %s", strerror(errno));
        return WLT_CRYPTO_ERR_IO;
    }
    /* Defend against a hostile umask: mkstemp uses 0600 & ~umask, which
     * on a weird umask could leave group/other bits set. Force 0600. */
    if (fchmod(kfd, 0600) < 0) {
        WLT_CRYPTO_LOG_ERR("luks: fchmod(keyfile) failed: %s", strerror(errno));
        close(kfd);
        unlink(out_path);
        return WLT_CRYPTO_ERR_IO;
    }
    /* Make sure the fd does not leak into the cryptsetup child. */
    if (fcntl(kfd, F_SETFD, FD_CLOEXEC) < 0) {
        WLT_CRYPTO_LOG_ERR("luks: fcntl(FD_CLOEXEC) failed: %s", strerror(errno));
        close(kfd);
        unlink(out_path);
        return WLT_CRYPTO_ERR_IO;
    }

    /* Stream the key bytes from key_fd into the keyfile. */
    char buf[4096];
    for (;;) {
        ssize_t r = read(key_fd, buf, sizeof(buf));
        if (r < 0) {
            if (errno == EINTR) continue;
            WLT_CRYPTO_LOG_ERR("luks: read(key_fd) failed: %s", strerror(errno));
            close(kfd);
            unlink(out_path);
            zero_buf(buf, sizeof(buf));
            return WLT_CRYPTO_ERR_IO;
        }
        if (r == 0) break;
        ssize_t off = 0;
        while (off < r) {
            ssize_t w = write(kfd, buf + off, (size_t)(r - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                WLT_CRYPTO_LOG_ERR("luks: write(keyfile) failed: %s", strerror(errno));
                close(kfd);
                unlink(out_path);
                zero_buf(buf, sizeof(buf));
                return WLT_CRYPTO_ERR_IO;
            }
            off += w;
        }
    }
    zero_buf(buf, sizeof(buf));

    if (fsync(kfd) < 0) {
        WLT_CRYPTO_LOG_ERR("luks: fsync(keyfile) failed: %s", strerror(errno));
        close(kfd);
        unlink(out_path);
        return WLT_CRYPTO_ERR_IO;
    }
    if (close(kfd) < 0) {
        WLT_CRYPTO_LOG_ERR("luks: close(keyfile) failed: %s", strerror(errno));
        unlink(out_path);
        return WLT_CRYPTO_ERR_IO;
    }
    return WLT_CRYPTO_OK;
}

/* remove_keyfile: zeroise the keyfile contents (best-effort on tmpfs),
 * fsync, then unlink. Operates on the unique path returned by
 * write_keyfile. Closes D-8: previously unlink() left the key bytes
 * recoverable from freed pages. */
static void
remove_keyfile(const char *path)
{
    if (path == NULL || path[0] == '\0')
        return;
    /* O_NOFOLLOW refuses symlinks: if an attacker swapped the keyfile
     * for a symlink between write_keyfile and here, we refuse to follow
     * it (and thus do not zeroise the target). */
    int fd = open(path, O_WRONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd >= 0) {
        struct stat st;
        if (fstat(fd, &st) == 0 && st.st_size > 0) {
            uint8_t zeros[LUKS_KEYFILE_ZEROS];
            memset(zeros, 0, sizeof(zeros));
            (void)lseek(fd, 0, SEEK_SET);
            size_t remaining = (size_t)st.st_size;
            while (remaining > 0) {
                size_t this_pass = (remaining < sizeof(zeros))
                                 ? remaining : sizeof(zeros);
                ssize_t w = write(fd, zeros, this_pass);
                if (w < 0) break;
                remaining -= (size_t)w;
            }
        }
        /* Truncate to 0 so the file's pre-unlink length is not visible
         * and any bytes beyond LUKS_KEYFILE_ZEROS are also gone. */
        if (ftruncate(fd, 0) < 0)
            WLT_CRYPTO_LOG_DBG("luks: ftruncate(keyfile) failed: %s",
                               strerror(errno));
        if (fsync(fd) < 0)
            WLT_CRYPTO_LOG_DBG("luks: fsync(keyfile) failed: %s",
                               strerror(errno));
        close(fd);
    }
    unlink(path);
}

wlt_crypto_error_t
wlt_crypto_luks_format_impl(const char *device, int key_fd)
{
    if (device == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    /* FAIL CLOSED: no key source -> refuse. The dispatcher in crypto.c
     * currently passes key_fd=-1 (KeyVaultPD integration pending) and
     * will get back INVALID_ARG here. */
    if (key_fd < 0)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    if (access(CRYPTSETUP_PATH, X_OK) != 0) {
        fprintf(stderr, "[wlt-crypto] cryptsetup not found at %s\n", CRYPTSETUP_PATH);
        return WLT_CRYPTO_ERR_UNSUPPORTED;
    }

    char keyfile[sizeof(LUKS_KEYFILE_TEMPLATE)];
    wlt_crypto_error_t err = write_keyfile(key_fd, keyfile, sizeof(keyfile));
    if (err != WLT_CRYPTO_OK)
        return err;

    char *argv[] = {
        (char *)CRYPTSETUP_PATH,
        "luksFormat",
        "--type", "luks2",
        "--key-file", keyfile,
        "--batch-mode",
        (char *)device,
        NULL
    };

    int exit_code = 0;
    err = run_cmd(CRYPTSETUP_PATH, argv, &exit_code);
    remove_keyfile(keyfile);

    if (err != WLT_CRYPTO_OK) {
        fprintf(stderr, "[wlt-crypto] luksFormat failed (exit %d)\n", exit_code);
        return WLT_CRYPTO_ERR_IO;
    }

    fprintf(stderr, "[wlt-crypto] LUKS2 format complete on %s\n", device);
    return WLT_CRYPTO_OK;
}

wlt_crypto_error_t
wlt_crypto_luks_open_impl(const char *device, const char *name, int key_fd)
{
    if (device == NULL || name == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;
    if (key_fd < 0)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    if (access(CRYPTSETUP_PATH, X_OK) != 0) {
        fprintf(stderr, "[wlt-crypto] cryptsetup not found\n");
        return WLT_CRYPTO_ERR_UNSUPPORTED;
    }

    char keyfile[sizeof(LUKS_KEYFILE_TEMPLATE)];
    wlt_crypto_error_t err = write_keyfile(key_fd, keyfile, sizeof(keyfile));
    if (err != WLT_CRYPTO_OK)
        return err;

    char *argv[] = {
        (char *)CRYPTSETUP_PATH,
        "luksOpen",
        "--key-file", keyfile,
        (char *)device,
        (char *)name,
        NULL
    };

    int exit_code = 0;
    err = run_cmd(CRYPTSETUP_PATH, argv, &exit_code);
    remove_keyfile(keyfile);

    if (err != WLT_CRYPTO_OK) {
        fprintf(stderr, "[wlt-crypto] luksOpen failed (exit %d)\n", exit_code);
        return WLT_CRYPTO_ERR_IO;
    }

    char mapper_path[256];
    snprintf(mapper_path, sizeof(mapper_path), "/dev/mapper/%s", name);
    if (access(mapper_path, F_OK) != 0) {
        fprintf(stderr, "[wlt-crypto] mapper device %s not created\n", mapper_path);
        return WLT_CRYPTO_ERR_IO;
    }

    fprintf(stderr, "[wlt-crypto] LUKS open: %s -> %s\n", device, mapper_path);
    return WLT_CRYPTO_OK;
}

wlt_crypto_error_t
wlt_crypto_luks_close_impl(const char *name)
{
    if (name == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    if (access(CRYPTSETUP_PATH, X_OK) != 0) {
        fprintf(stderr, "[wlt-crypto] cryptsetup not found\n");
        return WLT_CRYPTO_ERR_UNSUPPORTED;
    }

    char *argv[] = {
        (char *)CRYPTSETUP_PATH,
        "luksClose",
        (char *)name,
        NULL
    };

    int exit_code = 0;
    wlt_crypto_error_t err = run_cmd(CRYPTSETUP_PATH, argv, &exit_code);
    if (err != WLT_CRYPTO_OK) {
        fprintf(stderr, "[wlt-crypto] luksClose failed (exit %d)\n", exit_code);
        return WLT_CRYPTO_ERR_IO;
    }

    fprintf(stderr, "[wlt-crypto] LUKS close: %s\n", name);
    return WLT_CRYPTO_OK;
}

wlt_crypto_error_t
wlt_crypto_luks_status_impl(const char *name, bool *is_open)
{
    if (name == NULL || is_open == NULL)
        return WLT_CRYPTO_ERR_INVALID_ARG;

    char mapper_path[256];
    snprintf(mapper_path, sizeof(mapper_path), "/dev/mapper/%s", name);
    *is_open = (access(mapper_path, F_OK) == 0);
    return WLT_CRYPTO_OK;
}

wlt_crypto_error_t
wlt_crypto_kexec_wipe(void)
{
    /* STUB — structurally cannot work as written; refusing honestly.
     *
     * A correct kexec-based RAM wipe would:
     *   1. Load a wipe-kernel binary via kexec_load() (or kexec_file_load())
     *      with non-zero nr_segments + a real segments array.
     *   2. Call reboot(LINUX_REBOOT_CMD_KEXEC) to actually invoke it.
     *
     * The previous implementation instead called kexec_load with
     * nr_segments=0, which is the kexec UNLOAD opcode (it clears any
     * previously-loaded kernel, not loads one), then re-issued the same
     * syscall expecting it to "execute". It never called reboot(). So
     * the function could never actually wipe RAM — every call either
     * returned UNSUPPORTED (no kernel loaded, because step 1 was
     * missing) or returned IO with a misleading "exec failed" log
     * (after the unload syscall succeeded).
     *
     * Rather than ship a non-functional path that masquerades as a
     * secure-wipe, this stub returns WLT_CRYPTO_ERR_UNSUPPORTED until
     * a wipe-kernel binary + the load+reboot sequence is wired up.
     * Implementing that needs a real wipe-kernel image and is tracked
     * as a separate architectural task (see research-H in worklog).
     */
    fprintf(stderr,
            "[wlt-crypto] kexec wipe: stub — requires pre-loaded wipe-kernel "
            "binary + reboot(LINUX_REBOOT_CMD_KEXEC); refusing UNSUPPORTED\n");
    return WLT_CRYPTO_ERR_UNSUPPORTED;
}
