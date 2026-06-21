/**
 * @file config.c
 * @brief Boot config parsing implementation
 * @module wlt-boot/config
 * @purpose Parses /boot/wltios.conf INI-style config
 * @spec: docs/specs/wlt-boot.md
 */

#include "config.h"
#include "console.h"

wlt_boot_error_t
wlt_config_load(const char *path, wlt_boot_config_t *config)
{
    /* TODO(wlt-boot): Implement config file loading */
    /* Steps: */
    /*   1. Open file via UEFI SimpleFileSystemProtocol */
    /*   2. Read file into buffer */
    /*   3. Parse INI sections ([wlt-boot], [sel4]) */
    /*   4. Extract: kernel, signature, verify, cmdline, timeout */
    /*   5. Validate values */
    (void)path;
    wlt_config_get_default(config);
    return WLT_BOOT_OK;
}

void
wlt_config_get_default(wlt_boot_config_t *config)
{
    /* Default config if file missing */
    config->kernel_path[0] = '\0';
    /* TODO(wlt-boot): Copy default paths */
    config->signature_path[0] = '\0';
    config->cmdline[0] = '\0';
    config->verify_signature = true;
    config->timeout_ms = WLT_BOOT_TIMEOUT_MS;
}

wlt_boot_error_t
wlt_config_validate(const wlt_boot_config_t *config)
{
    if (config->kernel_path[0] == '\0')
    {
        return WLT_BOOT_ERR_PARSE;
    }
    return WLT_BOOT_OK;
}
