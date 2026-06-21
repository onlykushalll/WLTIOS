/**
 * @file config.h
 * @brief Boot config parsing for WLT-Boot
 * @module wlt-boot/config
 * @purpose Parses /boot/wltios.conf boot configuration
 * @dependencies: boot.h
 * @spec: docs/specs/wlt-boot.md
 */

#ifndef WLT_CONFIG_H
#define WLT_CONFIG_H

#include "boot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load boot config from file
 * @param path    Config file path (e.g., "\\boot\\wltios.conf")
 * @param config  Output config
 * @return WLT_BOOT_OK on success
 */
wlt_boot_error_t wlt_config_load(const char *path, wlt_boot_config_t *config);

/**
 * Get default config (used if file missing)
 */
void wlt_config_get_default(wlt_boot_config_t *config);

/**
 * Validate config values
 * @return WLT_BOOT_OK if valid
 */
wlt_boot_error_t wlt_config_validate(const wlt_boot_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* WLT_CONFIG_H */
