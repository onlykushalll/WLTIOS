/**
 * @file mount.c
 * @brief Filesystem mounting implementation
 * @module wlt-init/mount
 * @purpose Mounts essential filesystems at boot (proc, sys, dev, tmp, users)
 * @dependencies: mount.h, log.h, init.h
 * @tested_by: tests/userspace/mount_test.c
 * @spec: docs/specs/wlt-init.md
 */

#include "mount.h"
#include "log.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/* ─── Helpers ────────────────────────────────────────── */

static wlt_init_error_t
do_mount(const char *source, const char *target,
         const char *fstype, unsigned long flags,
         const void *data)
{
    /* Ensure mount point exists */
    if (mkdir(target, 0755) != 0 && errno != EEXIST)
    {
        WLT_LOG_ERR("mkdir %s failed: %s", target, strerror(errno));
        return WLT_INIT_ERR_IO;
    }

    if (mount(source, target, fstype, flags, data) != 0)
    {
        /* If already mounted, that's OK */
        if (errno == EBUSY)
        {
            WLT_LOG_DBG("%s already mounted", target);
            return WLT_INIT_OK;
        }
        WLT_LOG_ERR("mount %s failed: %s", target, strerror(errno));
        return WLT_INIT_ERR_IO;
    }

    WLT_LOG_INF("Mounted %s (%s) at %s", source, fstype, target);
    return WLT_INIT_OK;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_init_error_t
wlt_mount_essential(void)
{
    wlt_init_error_t err;

    /* 1. /proc — process information */
    WLT_LOG_INF("Mounting /proc...");
    err = do_mount("proc", "/proc", "proc", 0, NULL);
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_ERR("Failed to mount /proc");
        /* Continue anyway — not fatal for basic operation */
    }

    /* 2. /sys — system information */
    WLT_LOG_INF("Mounting /sys...");
    err = do_mount("sysfs", "/sys", "sysfs", 0, NULL);
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_ERR("Failed to mount /sys");
    }

    /* 3. /dev — device files */
    WLT_LOG_INF("Mounting /dev...");
    err = do_mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    if (err != WLT_INIT_OK)
    {
        /* Try tmpfs as fallback */
        WLT_LOG_WRN("devtmpfs failed, trying tmpfs for /dev");
        err = do_mount("tmpfs", "/dev", "tmpfs", 0, "mode=755");
    }

    /* 4. /dev/pts — pseudo-terminals (needed for terminal) */
    WLT_LOG_INF("Mounting /dev/pts...");
    mkdir("/dev/pts", 0755);
    do_mount("devpts", "/dev/pts", "devpts", 0, "gid=5,mode=620");

    /* 5. /dev/shm — shared memory */
    WLT_LOG_INF("Mounting /dev/shm...");
    mkdir("/dev/shm", 0755);
    do_mount("tmpfs", "/dev/shm", "tmpfs", 0, "mode=1777");

    /* 6. /tmp — temporary files (RAM-only, amnesic) */
    WLT_LOG_INF("Mounting /tmp (tmpfs, RAM-only)...");
    err = do_mount("tmpfs", "/tmp", "tmpfs", 0, "size=256M,mode=1777");
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_ERR("Failed to mount /tmp — this is critical");
        return err;
    }

    /* 7. /run — runtime data (RAM-only) */
    WLT_LOG_INF("Mounting /run (tmpfs, RAM-only)...");
    mkdir("/run", 0755);
    do_mount("tmpfs", "/run", "tmpfs", 0, "size=64M,mode=755");

    /* 8. /var/log — log directory (tmpfs, amnesic) */
    WLT_LOG_INF("Mounting /var/log (tmpfs, RAM-only)...");
    mkdir("/var/log", 0755);
    mkdir("/var/log/wlt", 0755);
    do_mount("tmpfs", "/var/log", "tmpfs", 0, "size=32M,mode=755");

    /* 9. /users — will be mounted later (needs Key Vault for encryption) */
    WLT_LOG_INF("Essential filesystems mounted");

    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_user(const char *username)
{
    if (username == NULL)
        return WLT_INIT_ERR_INVALID_ARG;

    char path[256];
    snprintf(path, sizeof(path), "/users/%s", username);

    /* Create mount point */
    mkdir("/users", 0755);
    if (mkdir(path, 0700) != 0 && errno != EEXIST)
    {
        WLT_LOG_ERR("mkdir %s failed: %s", path, strerror(errno));
        return WLT_INIT_ERR_IO;
    }

    /* TODO(wlt-init): Request Key Vault PD to unlock LUKS partition */
    /* For now, mount a tmpfs as placeholder */
    WLT_LOG_INF("Mounting user home for %s (tmpfs placeholder)", username);
    wlt_init_error_t err = do_mount("tmpfs", path, "tmpfs", 0,
                                     "size=512M,mode=700,uid=1000,gid=1000");
    if (err != WLT_INIT_OK)
    {
        WLT_LOG_ERR("Failed to mount user home for %s", username);
        return err;
    }

    /* Create standard directories */
    char subdir[300];
    const char *dirs[] = {"desktop", "documents", "downloads",
                          "music", "pictures", "videos", ".config", ".cache"};
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        snprintf(subdir, sizeof(subdir), "%s/%s", path, dirs[i]);
        mkdir(subdir, 0755);
    }

    WLT_LOG_INF("User home mounted: %s", path);
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_unmount_all(void)
{
    WLT_LOG_INF("Unmounting all filesystems...");

    /* Unmount in reverse order */
    const char *mounts[] = {
        "/users",      /* User data */
        "/var/log",    /* Logs */
        "/run",        /* Runtime */
        "/tmp",        /* Temp */
        "/dev/shm",    /* Shared memory */
        "/dev/pts",    /* PTY */
        "/dev",        /* Devices */
        "/sys",        /* System */
        "/proc",       /* Process */
    };

    for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); i++)
    {
        if (umount(mounts[i]) != 0)
        {
            WLT_LOG_WRN("umount %s: %s", mounts[i], strerror(errno));
            /* Try lazy unmount */
            umount2(mounts[i], MNT_DETACH);
        }
        else
        {
            WLT_LOG_DBG("Unmounted %s", mounts[i]);
        }
    }

    WLT_LOG_INF("All filesystems unmounted");
    return WLT_INIT_OK;
}

wlt_init_error_t
wlt_mount_root_readonly(void)
{
    WLT_LOG_INF("Remounting root as read-only...");

    if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL) != 0)
    {
        WLT_LOG_ERR("Failed to remount root read-only: %s", strerror(errno));
        return WLT_INIT_ERR_IO;
    }

    WLT_LOG_INF("Root is now read-only");
    return WLT_INIT_OK;
}
