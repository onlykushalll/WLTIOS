/**
 * @file partition.c
 * @brief A/B partition selection, boot config, and rollback
 * @module wlt-update/partition
 * @purpose Picks the inactive slot, formats/mounts it, extracts an
 *          update onto it, stages ESP boot artifacts, and rewrites
 *          /boot/wltios.conf so the next reboot is atomic + reversible.
 * @dependencies: update.h, partition.h, POSIX (fork/execve, statvfs)
 * @tested_by: tests/userspace/wlt_partition_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/07-custom-init.md
 */

#include "partition.h"
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

extern char **environ;

/* ââ Run an external command without a shell (style forbids system()). ââ */
static wlt_update_error_t
run_cmd(const char *file, char *const argv[], int *exit_code)
{
    if (file == NULL || argv == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    pid_t pid = fork();
    if (pid < 0) { WLT_UPDATE_LOG_ERR("fork: %s", strerror(errno)); return WLT_UPDATE_ERR_IO; }
    if (pid == 0) { execve(file, argv, environ); _exit(127); }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR) { return WLT_UPDATE_ERR_IO; }
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    if (exit_code != NULL) { *exit_code = code; }
    if (!WIFEXITED(status) || code != 0) { return WLT_UPDATE_ERR_IO; }
    return WLT_UPDATE_OK;
}

static wlt_update_error_t
read_text(const char *path, char *buf, size_t max)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { return WLT_UPDATE_ERR_NOT_FOUND; }
    ssize_t n = read(fd, buf, max - 1);
    close(fd);
    if (n < 0) { return WLT_UPDATE_ERR_IO; }
    buf[n] = '\0';
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) { buf[--n] = '\0'; }
    return WLT_UPDATE_OK;
}

static wlt_update_error_t
copy_file(const char *src, const char *dst)
{
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) { return WLT_UPDATE_ERR_NOT_FOUND; }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (out < 0) { WLT_UPDATE_LOG_ERR("create %s: %s", dst, strerror(errno)); close(in); return WLT_UPDATE_ERR_IO; }

    uint8_t buf[WLT_UPDATE_CHUNK_SIZE];
    ssize_t n;
    wlt_update_error_t err = WLT_UPDATE_OK;
    while ((n = read(in, buf, sizeof(buf))) > 0)
    {
        ssize_t off = 0;
        while (off < n)
        {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w < 0) { if (errno == EINTR) { continue; } err = WLT_UPDATE_ERR_IO; break; }
            off += w;
        }
        if (err != WLT_UPDATE_OK) { break; }
    }
    if (n < 0) { err = WLT_UPDATE_ERR_IO; }
    close(in);
    close(out);
    return err;
}

/* ââ True if @mountpoint currently has a filesystem mounted on it. ââ */
static bool
is_mounted(const char *mountpoint)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (fp == NULL) { return false; }
    char line[WLT_UPDATE_MAX_LINE * 2];
    bool found = false;
    size_t mplen = strlen(mountpoint);
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        /* /proc/mounts: "dev mountpoint fstype opts ..." */
        char *mp = strchr(line, ' ');
        if (mp == NULL) { continue; }
        mp++;
        if (strncmp(mp, mountpoint, mplen) == 0 &&
            (mp[mplen] == ' ' || mp[mplen] == '\0'))
        {
            found = true;
            break;
        }
    }
    fclose(fp);
    return found;
}

/* ââ Fill a BootConfig with the fixed slot layout defaults. ââ */
static void
fill_defaults(wlt_BootConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->version = 1;
    cfg->timeout = 3;
    cfg->default_partition = WLT_PARTITION_A;
    cfg->rollback_on_fail = true;

    static const char *devs[2] = { WLT_PARTITION_DEVICE_A, WLT_PARTITION_DEVICE_B };
    static const char *mnts[2] = { WLT_PARTITION_MOUNT_A, WLT_PARTITION_MOUNT_B };
    static const char *kern[2] = { WLT_PARTITION_KERNEL_A, WLT_PARTITION_KERNEL_B };
    static const char *lin[2]  = { WLT_PARTITION_LINUX_A, WLT_PARTITION_LINUX_B };
    static const char *sig[2]  = { WLT_PARTITION_SIG_A, WLT_PARTITION_SIG_B };
    for (int i = 0; i < WLT_PARTITION_COUNT; i++)
    {
        wlt_PartitionInfo *p = &cfg->partitions[i];
        p->id = (wlt_PartitionId)i;
        p->verify = true;
        wlt_strlcpy(p->device, devs[i], sizeof(p->device));
        wlt_strlcpy(p->mountpoint, mnts[i], sizeof(p->mountpoint));
        wlt_strlcpy(p->kernel, kern[i], sizeof(p->kernel));
        wlt_strlcpy(p->linux_img, lin[i], sizeof(p->linux_img));
        wlt_strlcpy(p->signature, sig[i], sizeof(p->signature));
    }
}

static void
apply_slot_key(wlt_PartitionInfo *p, const char *key, const char *val)
{
    if (strcmp(key, "kernel") == 0) { wlt_strlcpy(p->kernel, val, sizeof(p->kernel)); }
    else if (strcmp(key, "linux") == 0) { wlt_strlcpy(p->linux_img, val, sizeof(p->linux_img)); }
    else if (strcmp(key, "signature") == 0) { wlt_strlcpy(p->signature, val, sizeof(p->signature)); }
    else if (strcmp(key, "verify") == 0) { p->verify = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0); }
    else if (strcmp(key, "last_boot_success") == 0) { wlt_strlcpy(p->last_boot_success, val, sizeof(p->last_boot_success)); }
    else if (strcmp(key, "boot_attempts") == 0) { p->boot_attempts = (int)strtol(val, NULL, 10); }
    else if (strcmp(key, "failed") == 0) { p->failed = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0); }
    else if (strcmp(key, "version") == 0) { wlt_strlcpy(p->version, val, sizeof(p->version)); }
}

/* ââ Public: load /boot/wltios.conf ââ */
wlt_update_error_t
wlt_boot_config_load(const char *path, wlt_BootConfig *out)
{
    if (path == NULL || out == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    FILE *fp = fopen(path, "r");
    if (fp == NULL) { WLT_UPDATE_LOG_ERR("boot config %s: %s", path, strerror(errno)); return WLT_UPDATE_ERR_NOT_FOUND; }

    fill_defaults(out);
    char line[WLT_UPDATE_MAX_LINE], key[WLT_UPDATE_MAX_LINE], val[WLT_UPDATE_MAX_LINE];
    int section = 0; /* 0 none, 1 [wlt-boot], 2 [A], 3 [B] */

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (line[0] == '[')
        {
            if (strncmp(line, "[wlt-boot]", 10) == 0) { section = 1; }
            else if (strncmp(line, "[A]", 3) == 0) { section = 2; }
            else if (strncmp(line, "[B]", 3) == 0) { section = 3; }
            else { section = 0; }
            continue;
        }
        if (!wlt_config_parse_kv(line, key, sizeof(key), val, sizeof(val))) { continue; }

        if (section == 1)
        {
            if (strcmp(key, "version") == 0) { out->version = (uint32_t)strtoul(val, NULL, 10); }
            else if (strcmp(key, "timeout") == 0) { out->timeout = (uint32_t)strtoul(val, NULL, 10); }
            else if (strcmp(key, "default") == 0) { out->default_partition = (val[0] == 'B') ? WLT_PARTITION_B : WLT_PARTITION_A; }
            else if (strcmp(key, "rollback_on_fail") == 0) { out->rollback_on_fail = (strcmp(val, "yes") == 0 || strcmp(val, "true") == 0); }
        }
        else if (section == 2) { apply_slot_key(&out->partitions[WLT_PARTITION_A], key, val); }
        else if (section == 3) { apply_slot_key(&out->partitions[WLT_PARTITION_B], key, val); }
    }
    fclose(fp);
    return WLT_UPDATE_OK;
}

/* ââ Public: atomically save /boot/wltios.conf ââ */
wlt_update_error_t
wlt_boot_config_save(const char *path, const wlt_BootConfig *cfg)
{
    if (path == NULL || cfg == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    char tmp[WLT_UPDATE_MAX_PATH];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) { return WLT_UPDATE_ERR_INVALID_ARG; }
    FILE *fp = fopen(tmp, "w");
    if (fp == NULL) { WLT_UPDATE_LOG_ERR("write %s: %s", tmp, strerror(errno)); return WLT_UPDATE_ERR_IO; }

    fprintf(fp, "# WLT-Boot configuration â written by wlt-update\n[wlt-boot]\n");
    fprintf(fp, "version = %u\ntimeout = %u\n", cfg->version, cfg->timeout);
    fprintf(fp, "default = %c\n", cfg->default_partition == WLT_PARTITION_B ? 'B' : 'A');
    fprintf(fp, "rollback_on_fail = %s\n", cfg->rollback_on_fail ? "yes" : "no");
    for (int i = 0; i < WLT_PARTITION_COUNT; i++)
    {
        const wlt_PartitionInfo *p = &cfg->partitions[i];
        fprintf(fp, "\n[%c]\n", p->id == WLT_PARTITION_B ? 'B' : 'A');
        fprintf(fp, "kernel = %s\n", p->kernel);
        fprintf(fp, "linux = %s\n", p->linux_img);
        fprintf(fp, "signature = %s\n", p->signature);
        fprintf(fp, "verify = %s\n", p->verify ? "yes" : "no");
        fprintf(fp, "last_boot_success = %s\n", p->last_boot_success[0] ? p->last_boot_success : "never");
        fprintf(fp, "boot_attempts = %d\n", p->boot_attempts);
        fprintf(fp, "failed = %s\n", p->failed ? "yes" : "no");
        fprintf(fp, "version = %s\n", p->version[0] ? p->version : "unknown");
    }
    if (fclose(fp) != 0) { return WLT_UPDATE_ERR_IO; }
    if (rename(tmp, path) != 0) { WLT_UPDATE_LOG_ERR("rename %s: %s", path, strerror(errno)); return WLT_UPDATE_ERR_IO; }
    return WLT_UPDATE_OK;
}

/* ââ Public: slot selection ââ */
wlt_update_error_t
wlt_partition_get_active(wlt_PartitionId *out)
{
    if (out == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    char buf[8];
    if (read_text(WLT_UPDATE_ACTIVE_MARKER, buf, sizeof(buf)) == WLT_UPDATE_OK &&
        (buf[0] == 'A' || buf[0] == 'B'))
    {
        *out = (buf[0] == 'B') ? WLT_PARTITION_B : WLT_PARTITION_A;
        return WLT_UPDATE_OK;
    }
    wlt_BootConfig cfg;
    if (wlt_boot_config_load(WLT_UPDATE_BOOT_CONFIG_PATH, &cfg) == WLT_UPDATE_OK)
    {
        *out = cfg.default_partition;
    }
    else
    {
        *out = WLT_PARTITION_A;
    }
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_partition_get_inactive(wlt_PartitionId *out)
{
    if (out == NULL) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_PartitionId active;
    wlt_update_error_t err = wlt_partition_get_active(&active);
    if (err != WLT_UPDATE_OK) { return err; }
    *out = (active == WLT_PARTITION_A) ? WLT_PARTITION_B : WLT_PARTITION_A;
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_partition_get_info(wlt_PartitionId id, wlt_PartitionInfo *out)
{
    if (out == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    wlt_update_error_t err = wlt_boot_config_load(WLT_UPDATE_BOOT_CONFIG_PATH, &cfg);
    if (err != WLT_UPDATE_OK) { return err; }
    *out = cfg.partitions[id];
    return WLT_UPDATE_OK;
}

/* ââ Public: format / mount / unmount / free space ââ */
static const wlt_PartitionInfo *
slot_info(wlt_PartitionId id, wlt_BootConfig *cfg)
{
    if (wlt_boot_config_load(WLT_UPDATE_BOOT_CONFIG_PATH, cfg) != WLT_UPDATE_OK)
    {
        fill_defaults(cfg);
    }
    return &cfg->partitions[id];
}

wlt_update_error_t
wlt_partition_format(wlt_PartitionId id)
{
    if (id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    const wlt_PartitionInfo *p = slot_info(id, &cfg);
    char *argv[] = { (char *)"mkfs.ext4", (char *)"-F", (char *)p->device, NULL };
    WLT_UPDATE_LOG_INF("formatting %s (slot %c)", p->device, id == WLT_PARTITION_B ? 'B' : 'A');
    return run_cmd(WLT_PARTITION_MKFS, argv, NULL);
}

wlt_update_error_t
wlt_partition_mount(wlt_PartitionId id)
{
    if (id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    const wlt_PartitionInfo *p = slot_info(id, &cfg);
    if (is_mounted(p->mountpoint)) { return WLT_UPDATE_OK; }
    char *argv[] = { (char *)"mount", (char *)p->device, (char *)p->mountpoint, NULL };
    wlt_update_error_t err = run_cmd(WLT_PARTITION_MOUNT, argv, NULL);
    if (err != WLT_UPDATE_OK) { WLT_UPDATE_LOG_ERR("mount %s on %s failed", p->device, p->mountpoint); }
    return err;
}

wlt_update_error_t
wlt_partition_unmount(wlt_PartitionId id)
{
    if (id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    const wlt_PartitionInfo *p = slot_info(id, &cfg);
    if (!is_mounted(p->mountpoint)) { return WLT_UPDATE_OK; }
    char *argv[] = { (char *)"umount", (char *)p->mountpoint, NULL };
    int code = 0;
    (void)run_cmd(WLT_PARTITION_UMOUNT, argv, &code);
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_partition_get_free_space(wlt_PartitionId id, uint64_t *bytes_out)
{
    if (bytes_out == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    const wlt_PartitionInfo *p = slot_info(id, &cfg);
    struct statvfs vfs;
    if (statvfs(p->mountpoint, &vfs) != 0)
    {
        WLT_UPDATE_LOG_ERR("statvfs %s: %s", p->mountpoint, strerror(errno));
        return WLT_UPDATE_ERR_IO;
    }
    *bytes_out = (uint64_t)vfs.f_frsize * (uint64_t)vfs.f_bavail;
    return WLT_UPDATE_OK;
}

/* ââ Public: extract a package onto a mounted slot + stage ESP files ââ */
wlt_update_error_t
wlt_partition_install_to(const char *package_path, wlt_PartitionId id)
{
    if (package_path == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_BootConfig cfg;
    const wlt_PartitionInfo *p = slot_info(id, &cfg);
    const char *mp = p->mountpoint;

    char opt[WLT_UPDATE_MAX_PATH];
    snprintf(opt, sizeof(opt), "--use-compress-program=%s", WLT_PARTITION_ZSTD);
    char *argv[] = { (char *)"tar", opt, (char *)"-xf",
                     (char *)package_path, (char *)"-C", (char *)mp, NULL };
    WLT_UPDATE_LOG_INF("extracting %s onto slot %c", package_path, id == WLT_PARTITION_B ? 'B' : 'A');
    wlt_update_error_t err = run_cmd(WLT_PARTITION_TAR, argv, NULL);
    if (err != WLT_UPDATE_OK) { WLT_UPDATE_LOG_ERR("extraction failed"); return err; }

    char src[WLT_UPDATE_MAX_PATH], dst[WLT_UPDATE_MAX_PATH];
    char letter = (id == WLT_PARTITION_B) ? 'B' : 'A';
    /* kernel + signature are required for verified boot. */
    snprintf(src, sizeof(src), "%s/%s", mp, WLT_PARTITION_PAYLOAD_KERNEL);
    snprintf(dst, sizeof(dst), "/boot/sel4-%c.bin", letter);
    if (copy_file(src, dst) != WLT_UPDATE_OK) { WLT_UPDATE_LOG_ERR("missing kernel (%s)", src); return WLT_UPDATE_ERR_IO; }
    snprintf(src, sizeof(src), "%s/%s", mp, WLT_PARTITION_PAYLOAD_SIG);
    snprintf(dst, sizeof(dst), "/boot/sel4-%c.sig", letter);
    if (copy_file(src, dst) != WLT_UPDATE_OK) { WLT_UPDATE_LOG_ERR("missing kernel signature (%s)", src); return WLT_UPDATE_ERR_IO; }
    /* linux image is optional on some seL4 layouts. */
    snprintf(src, sizeof(src), "%s/%s", mp, WLT_PARTITION_PAYLOAD_LINUX);
    snprintf(dst, sizeof(dst), "/boot/linux-%c.img", letter);
    if (copy_file(src, dst) != WLT_UPDATE_OK) { WLT_UPDATE_LOG_WRN("no linux image; kernel-only boot"); }
    return WLT_UPDATE_OK;
}

/* ââ Public: boot config mutators ââ */
wlt_update_error_t
wlt_boot_config_set_default(wlt_BootConfig *cfg, wlt_PartitionId id)
{
    if (cfg == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    cfg->default_partition = id;
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_boot_config_mark_success(wlt_BootConfig *cfg, wlt_PartitionId id)
{
    if (cfg == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    wlt_PartitionInfo *p = &cfg->partitions[id];
    p->failed = false;
    p->boot_attempts = 0;
    time_t t = time(NULL);
    struct tm tmv;
    if (gmtime_r(&t, &tmv) != NULL)
    {
        strftime(p->last_boot_success, sizeof(p->last_boot_success),
                 "%Y-%m-%dT%H:%M:%SZ", &tmv);
    }
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_boot_config_mark_failed(wlt_BootConfig *cfg, wlt_PartitionId id)
{
    if (cfg == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    cfg->partitions[id].failed = true;
    return WLT_UPDATE_OK;
}

wlt_update_error_t
wlt_boot_config_increment_attempts(wlt_BootConfig *cfg, wlt_PartitionId id)
{
    if (cfg == NULL || id >= WLT_PARTITION_COUNT) { return WLT_UPDATE_ERR_INVALID_ARG; }
    if (++cfg->partitions[id].boot_attempts >= WLT_UPDATE_MAX_BOOT_ATTEMPTS)
    {
        cfg->partitions[id].failed = true;
    }
    return WLT_UPDATE_OK;
}

/* ââ Public: rollback (swap default to the other, non-failed slot) ââ */
wlt_update_error_t
wlt_partition_rollback(void)
{
    wlt_PartitionId active;
    wlt_update_error_t err = wlt_partition_get_active(&active);
    if (err != WLT_UPDATE_OK) { return err; }
    wlt_PartitionId target = (active == WLT_PARTITION_A) ? WLT_PARTITION_B : WLT_PARTITION_A;

    wlt_BootConfig cfg;
    err = wlt_boot_config_load(WLT_UPDATE_BOOT_CONFIG_PATH, &cfg);
    if (err != WLT_UPDATE_OK) { return err; }

    /* No-downgrade / no-bricking: refuse to boot a known-failed slot. */
    if (cfg.partitions[target].failed)
    {
        WLT_UPDATE_LOG_ERR("rollback target slot %c is marked failed",
                           target == WLT_PARTITION_B ? 'B' : 'A');
        return WLT_UPDATE_ERR_PARTITION;
    }
    wlt_boot_config_set_default(&cfg, target);
    err = wlt_boot_config_save(WLT_UPDATE_BOOT_CONFIG_PATH, &cfg);
    if (err != WLT_UPDATE_OK) { return err; }
    WLT_UPDATE_LOG_INF("rollback staged: next boot from slot %c",
                       target == WLT_PARTITION_B ? 'B' : 'A');
    return WLT_UPDATE_OK;
}
