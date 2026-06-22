/**
 * @file partition.h
 * @brief A/B partition management for WLT-Update
 * @module wlt-update/partition
 * @purpose Selects the active/inactive OS partition, formats and mounts
 *          the inactive slot, stages boot files on the ESP, parses and
 *          rewrites the WLT-Boot config, and drives rollback. An update
 *          only becomes "live" once the boot default flips and a reboot
 *          succeeds â everything here is reversible.
 * @dependencies: update.h
 * @tested_by: tests/userspace/wlt_partition_test.c
 * @spec: docs/specs/wlt-update.md
 * @adr: docs/adr/07-custom-init.md
 */

#ifndef WLT_PARTITION_H
#define WLT_PARTITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "update.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_PARTITION_A = 0,
    WLT_PARTITION_B = 1,
} wlt_PartitionId;

#define WLT_PARTITION_COUNT 2

/* LUKS2-mapped root devices (unlocked by WLT-Init at boot). */
#define WLT_PARTITION_DEVICE_A "/dev/mapper/wlt-root-a"
#define WLT_PARTITION_DEVICE_B "/dev/mapper/wlt-root-b"
#define WLT_PARTITION_MOUNT_A  "/mnt/wlt-a"
#define WLT_PARTITION_MOUNT_B  "/mnt/wlt-b"

/* ESP boot artifacts, one set per slot. */
#define WLT_PARTITION_KERNEL_A "/boot/sel4-a.bin"
#define WLT_PARTITION_KERNEL_B "/boot/sel4-b.bin"
#define WLT_PARTITION_LINUX_A  "/boot/linux-a.img"
#define WLT_PARTITION_LINUX_B  "/boot/linux-b.img"
#define WLT_PARTITION_SIG_A    "/boot/sel4-a.sig"
#define WLT_PARTITION_SIG_B    "/boot/sel4-b.sig"

/* Files staged inside a freshly extracted partition. */
#define WLT_PARTITION_PAYLOAD_KERNEL "boot/sel4.bin"
#define WLT_PARTITION_PAYLOAD_LINUX  "boot/linux.img"
#define WLT_PARTITION_PAYLOAD_SIG    "boot/sel4.sig"

/* External helpers invoked via execve (never system()). */
#define WLT_PARTITION_MKFS    "/usr/sbin/mkfs.ext4"
#define WLT_PARTITION_MOUNT   "/bin/mount"
#define WLT_PARTITION_UMOUNT  "/bin/umount"
#define WLT_PARTITION_TAR     "/bin/tar"
#define WLT_PARTITION_ZSTD    "/usr/bin/zstd"

/* ââ Types ââ */

/** Per-slot state, mirrored from /boot/wltios.conf. */
typedef struct {
    wlt_PartitionId id;
    char device[WLT_UPDATE_MAX_PATH];     /* mapped block device          */
    char mountpoint[WLT_UPDATE_MAX_PATH];
    char kernel[WLT_UPDATE_MAX_PATH];     /* ESP kernel image path        */
    char linux_img[WLT_UPDATE_MAX_PATH];  /* ESP linux image path         */
    char signature[WLT_UPDATE_MAX_PATH];  /* ESP signature path           */
    bool verify;                          /* bootloader verifies boot     */
    char last_boot_success[32];           /* ISO-8601, "" if never        */
    int  boot_attempts;
    bool failed;                          /* marked failed by bootloader  */
    char version[WLT_UPDATE_MAX_VERSION];
} wlt_PartitionInfo;

/** In-memory image of /boot/wltios.conf. */
typedef struct {
    uint32_t           version;
    uint32_t           timeout;
    wlt_PartitionId    default_partition;
    bool               rollback_on_fail;
    wlt_PartitionInfo  partitions[WLT_PARTITION_COUNT];
} wlt_BootConfig;

/* ââ Slot selection ââ */
wlt_update_error_t wlt_partition_get_active(wlt_PartitionId *out);
wlt_update_error_t wlt_partition_get_inactive(wlt_PartitionId *out);
wlt_update_error_t wlt_partition_get_info(wlt_PartitionId id,
                                          wlt_PartitionInfo *out);

/* ââ Slot manipulation ââ */
wlt_update_error_t wlt_partition_format(wlt_PartitionId id);
wlt_update_error_t wlt_partition_mount(wlt_PartitionId id);
wlt_update_error_t wlt_partition_unmount(wlt_PartitionId id);
wlt_update_error_t wlt_partition_get_free_space(wlt_PartitionId id,
                                                uint64_t *bytes_out);

/** Extract a .wlt-update.tar.zst onto a mounted slot + stage ESP files. */
wlt_update_error_t wlt_partition_install_to(const char *package_path,
                                            wlt_PartitionId id);

/* ââ Boot config ââ */
wlt_update_error_t wlt_boot_config_load(const char *path,
                                        wlt_BootConfig *out);
wlt_update_error_t wlt_boot_config_save(const char *path,
                                        const wlt_BootConfig *cfg);
wlt_update_error_t wlt_boot_config_set_default(wlt_BootConfig *cfg,
                                               wlt_PartitionId id);
wlt_update_error_t wlt_boot_config_mark_success(wlt_BootConfig *cfg,
                                                wlt_PartitionId id);
wlt_update_error_t wlt_boot_config_mark_failed(wlt_BootConfig *cfg,
                                               wlt_PartitionId id);
wlt_update_error_t wlt_boot_config_increment_attempts(wlt_BootConfig *cfg,
                                                      wlt_PartitionId id);

/* ââ Rollback ââ */
wlt_update_error_t wlt_partition_rollback(void);

#ifdef __cplusplus
}
#endif

#endif /* WLT_PARTITION_H */
