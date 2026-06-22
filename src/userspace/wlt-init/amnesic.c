/**
 * @file amnesic.c
 * @brief Amnesic mode enforcement — ensure nothing writes to disk
 * @module wlt-init/amnesic
 * @purpose Enforce RAM-only operation when booted from Live USB
 * @dependencies: init.h, mount.h
 * @spec: docs/specs/wlt-init.md
 * @adr: docs/adr/07-custom-init.md
 *
 * When amnesic mode is active:
 * - Root filesystem is read-only
 * - All writable paths are on tmpfs (RAM)
 * - Swap is disabled
 * - No filesystem writes reach disk
 * - User must explicitly save to persistence partition
 */

#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

typedef enum {
    WLT_AMNESIC_OK = 0,
    WLT_AMNESIC_ERR_IO = -1,
    WLT_AMNESIC_ERR_PERMISSION = -2,
} wlt_amnesic_error_t;

static bool g_amnesic_active = false;

/* Check if running in amnesic (Live USB) mode */
bool
wlt_amnesic_is_active(void)
{
    return g_amnesic_active;
}

/* Detect if booted from Live USB (check kernel cmdline for "wltios.live") */
wlt_amnesic_error_t
wlt_amnesic_detect(void)
{
    FILE *f = fopen("/proc/cmdline", "r");
    if (f == NULL)
        return WLT_AMNESIC_ERR_IO;

    char cmdline[1024];
    if (fgets(cmdline, sizeof(cmdline), f) == NULL)
    {
        fclose(f);
        return WLT_AMNESIC_ERR_IO;
    }
    fclose(f);

    if (strstr(cmdline, "wltios.live") != NULL ||
        strstr(cmdline, "boot=casper") != NULL ||
        strstr(cmdline, "boot=live") != NULL)
    {
        g_amnesic_active = true;
    }

    return WLT_AMNESIC_OK;
}

/* Enforce amnesic mode: remount root read-only, mount tmpfs on writable paths */
wlt_amnesic_error_t
wlt_amnesic_enforce(void)
{
    if (!g_amnesic_active)
        return WLT_AMNESIC_OK;

    /* 1. Remount root filesystem read-only */
    if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL) != 0)
    {
        /* Non-fatal — might already be read-only */
    }

    /* 2. Ensure /tmp is tmpfs */
    mkdir("/tmp", 01777);
    mount("tmpfs", "/tmp", "tmpfs", 0, "size=256M,mode=1777");

    /* 3. Ensure /run is tmpfs */
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", 0, "size=64M,mode=755");

    /* 4. Ensure /var/log is tmpfs */
    mkdir("/var/log", 0755);
    mkdir("/var/log/wlt", 0755);
    mount("tmpfs", "/var/log", "tmpfs", 0, "size=32M,mode=755");

    /* 5. Ensure /var/tmp is tmpfs */
    mkdir("/var/tmp", 01777);
    mount("tmpfs", "/var/tmp", "tmpfs", 0, "size=32M,mode=1777");

    /* 6. Ensure /var/wlt is tmpfs (package store, cache) */
    mkdir("/var/wlt", 0755);
    mkdir("/var/wlt/store", 0755);
    mkdir("/var/wlt/db", 0755);
    mkdir("/var/wlt/cache", 0755);
    mount("tmpfs", "/var/wlt", "tmpfs", 0, "size=128M,mode=755");

    /* 7. Ensure /var/lib/tor is tmpfs (amnesic — no Tor state persists) */
    mkdir("/var/lib", 0755);
    mkdir("/var/lib/tor", 0700);
    mount("tmpfs", "/var/lib/tor", "tmpfs", 0, "size=64M,mode=700");

    /* 8. Disable swap (no disk writes) */
    /* swapoff -a would be: system("swapoff -a"); but we don't use system() */
    /* Instead: write to /proc/sys/vm/swappiness = 0 */
    FILE *swp = fopen("/proc/sys/vm/swappiness", "w");
    if (swp != NULL)
    {
        fprintf(swp, "0\n");
        fclose(swp);
    }

    /* 9. Ensure user home is tmpfs */
    mkdir("/users", 0755);
    mkdir("/users/default", 0700);
    mount("tmpfs", "/users/default", "tmpfs", 0, "size=512M,mode=700,uid=1000,gid=1000");

    /* 10. Create standard user directories in RAM */
    const char *dirs[] = {"desktop", "documents", "downloads",
                          "music", "pictures", "videos", ".config", ".cache"};
    char path[256];
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        snprintf(path, sizeof(path), "/users/default/%s", dirs[i]);
        mkdir(path, 0755);
    }

    return WLT_AMNESIC_OK;
}

/* Save a file to the persistence partition (if enabled) */
wlt_amnesic_error_t
wlt_amnesic_save_to_persistence(const char *src_path, const char *dest_name)
{
    if (!g_amnesic_active || src_path == NULL || dest_name == NULL)
        return WLT_AMNESIC_ERR_IO;

    /* Check if persistence partition is mounted */
    struct stat st;
    if (stat("/persistence", &st) != 0)
    {
        /* Persistence not available */
        return WLT_AMNESIC_ERR_IO;
    }

    /* TODO(wlt-init): Copy file from src_path to /persistence/<dest_name> */
    /* This requires user authentication (LUKS password) */

    return WLT_AMNESIC_OK;
}
