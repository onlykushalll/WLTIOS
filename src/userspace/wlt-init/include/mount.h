/**
 * @file mount.h
 * @brief Filesystem mounting for WLT-Init
 * @module wlt-init/mount
 * @purpose Mounts essential filesystems at boot
 * @dependencies: init.h
 * @spec: docs/specs/wlt-init.md
 */

#ifndef WLT_MOUNT_H
#define WLT_MOUNT_H

#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mount all essential filesystems:
 *   - /proc, /sys, /dev
 *   - /tmp (tmpfs, RAM-only)
 *   - /users (encrypted, via Key Vault)
 *   - /var (if persistent)
 */
wlt_init_error_t wlt_mount_essential(void);

/**
 * Mount the user home directory (encrypted)
 * Requires Key Vault PD to be available
 */
wlt_init_error_t wlt_mount_user(const char *username);

/**
 * Unmount all filesystems (for shutdown)
 */
wlt_init_error_t wlt_mount_unmount_all(void);

/**
 * Remount root as read-only
 */
wlt_init_error_t wlt_mount_root_readonly(void);

#ifdef __cplusplus
}
#endif

#endif /* WLT_MOUNT_H */
