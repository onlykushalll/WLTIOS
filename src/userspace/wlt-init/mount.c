/**
 * @file mount.c
 * @brief Filesystem mounting
 * @module wlt-init/mount
 * @purpose Mount essential filesystems at boot
 * @spec: docs/specs/wlt-init.md
 */

#include "mount.h"
#include "log.h"
#include <sys/mount.h>
#include <string.h>

wlt_init_error_t
wlt_mount_essential(void)
{
    /* TODO(wlt-init): Implement mounting */
    /* Mount: */
    /*   1. proc    → /proc    (proc filesystem) */
    /*   2. sysfs   → /sys     (sys filesystem) */
    /*   3. devtmpfs → /dev    (device filesystem) */
    /*   4. tmpfs   → /tmp     (RAM-only, no disk writes) */
    /*   5. /users  → encrypted partition (via Key Vault PD) */

    WLT_LOG_INF("Mounting /proc...");
    /* mount("proc", "/proc", "proc", 0, NULL); */

    WLT_LOG_INF("Mounting /sys...");
    /* mount("sysfs", "/sys", "sysfs", 0, NULL); */

    WLT_LOG_INF("Mounting /dev...");
    /* mount("devtmpfs", "/dev", "devtmpfs", 0, NULL); */

    WLT_LOG_INF("Mounting /tmp (tmpfs, RAM-only)...");
    /* mount("tmpfs", "/tmp", "tmpfs", 0, "size=256M"); */

    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_user(const char *username)
{
    /* TODO(wlt-init): Mount encrypted user partition */
    /* Steps: */
    /*   1. Request Key Vault PD to unlock LUKS partition */
    /*   2. mount decrypted partition to /users/<username> */
    (void)username;
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_unmount_all(void)
{
    /* TODO(wlt-init): Unmount in reverse order */
    /* umount /users, /tmp, /dev, /sys, /proc */
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_root_readonly(void)
{
    /* mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL); */
    return WLT_INIT_OK;
}
