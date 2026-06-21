/**
 * @file config.c
 * @brief Boot config parsing implementation
 * @module wlt-boot/config
 * @purpose Parses /boot/wltios.conf INI-style config file
 * @dependencies: config.h, console.h
 * @tested_by: tests/boot/config_test.c
 * @spec: docs/specs/wlt-boot.md
 */

#include "config.h"
#include "console.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ─── INI Parser Helpers ─────────────────────────────── */

static char *
trim(char *str)
{
    /* Find first non-space */
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;

    /* Shift string to remove leading spaces */
    if (start != str)
        memmove(str, start, strlen(start) + 1);

    /* Trim trailing spaces */
    if (*str)
    {
        char *end = str + strlen(str) - 1;
        while (end > str && isspace((unsigned char)*end))
            *end-- = '\0';
    }
    return str;
}

static int
parse_kv(const char *line, char *key, int key_max, char *val, int val_max)
{
    const char *eq = strchr(line, '=');
    if (eq == NULL)
        return -1;

    /* Extract key (before =) */
    int klen = eq - line;
    if (klen >= key_max)
        klen = key_max - 1;
    strncpy(key, line, klen);
    key[klen] = '\0';
    trim(key);

    /* Extract value (after =) */
    const char *vstart = eq + 1;
    int vlen = strlen(vstart);
    if (vlen >= val_max)
        vlen = val_max - 1;
    strncpy(val, vstart, vlen);
    val[vlen] = '\0';
    trim(val);

    return 0;
}

static void
set_config_value(wlt_boot_config_t *config,
                  const char *section,
                  const char *key,
                  const char *val)
{
    if (strcmp(section, "wlt-boot") == 0)
    {
        if (strcmp(key, "timeout") == 0)
        {
            config->timeout_ms = (uint32_t)(atoi(val) * 1000);
        }
        /* Other wlt-boot keys: default, version */
    }
    else if (strcmp(section, "sel4") == 0 || strcmp(section, "default") == 0)
    {
        if (strcmp(key, "kernel") == 0)
        {
            strncpy(config->kernel_path, val, WLT_BOOT_MAX_PATH - 1);
            config->kernel_path[WLT_BOOT_MAX_PATH - 1] = '\0';
        }
        else if (strcmp(key, "signature") == 0)
        {
            strncpy(config->signature_path, val, WLT_BOOT_MAX_PATH - 1);
            config->signature_path[WLT_BOOT_MAX_PATH - 1] = '\0';
        }
        else if (strcmp(key, "verify") == 0)
        {
            config->verify_signature = (strcmp(val, "yes") == 0 ||
                                         strcmp(val, "true") == 0 ||
                                         strcmp(val, "1") == 0);
        }
        else if (strcmp(key, "cmdline") == 0)
        {
            /* Remove quotes if present */
            const char *v = val;
            if (*v == '"')
            {
                v++;
                char *end = strchr(v, '"');
                int len = end ? (int)(end - v) : (int)strlen(v);
                strncpy(config->cmdline, v, sizeof(config->cmdline) - 1);
                config->cmdline[len] = '\0';
            }
            else
            {
                strncpy(config->cmdline, val, sizeof(config->cmdline) - 1);
                config->cmdline[sizeof(config->cmdline) - 1] = '\0';
            }
        }
    }
}

/* ─── Public API ─────────────────────────────────────── */

wlt_boot_error_t
wlt_config_load(const char *path, wlt_boot_config_t *config)
{
    if (path == NULL || config == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    /* Start with defaults */
    wlt_config_get_default(config);

    /* TODO(wlt-boot): Read file via UEFI SimpleFileSystemProtocol */
    /* For now, we just use defaults — file reading requires UEFI boot services */
    /* The actual file reading will be implemented when we have UEFI context */
    /* (the efi_main function provides the boot services table) */

    /* When file reading is implemented, the parsing logic is: */
    /*
    char buf[WLT_BOOT_MAX_CONFIG];
    // Read file into buf via UEFI
    if (read_failed) return WLT_BOOT_ERR_IO;

    char section[64] = "";
    char *line = buf;
    while (line < buf + sizeof(buf))
    {
        // Get one line
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *trimmed = trim(line);

        // Skip empty lines and comments
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            goto next;

        // Section header [section]
        if (*trimmed == '[')
        {
            char *end = strchr(trimmed, ']');
            if (end)
            {
                *end = '\0';
                strncpy(section, trimmed + 1, sizeof(section) - 1);
                section[sizeof(section) - 1] = '\0';
            }
            goto next;
        }

        // Key = value
        char key[128], val[256];
        if (parse_kv(trimmed, key, sizeof(key), val, sizeof(val)) == 0)
        {
            set_config_value(config, section, key, val);
        }

    next:
        if (nl)
            line = nl + 1;
        else
            break;
    }
    */

    wlt_console_info("Config loaded (using defaults — file reading TODO)");
    return WLT_BOOT_OK;
}

void
wlt_config_get_default(wlt_boot_config_t *config)
{
    if (config == NULL)
        return;

    /* Default paths on EFI System Partition */
    strncpy(config->kernel_path, "\\boot\\sel4.bin",
            WLT_BOOT_MAX_PATH - 1);
    config->kernel_path[WLT_BOOT_MAX_PATH - 1] = '\0';

    strncpy(config->signature_path, "\\boot\\sel4.sig",
            WLT_BOOT_MAX_PATH - 1);
    config->signature_path[WLT_BOOT_MAX_PATH - 1] = '\0';

    strncpy(config->cmdline, "console=serial",
            sizeof(config->cmdline) - 1);
    config->cmdline[sizeof(config->cmdline) - 1] = '\0';

    config->verify_signature = true;
    config->timeout_ms = WLT_BOOT_TIMEOUT_MS;
}

wlt_boot_error_t
wlt_config_validate(const wlt_boot_config_t *config)
{
    if (config == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    if (config->kernel_path[0] == '\0')
    {
        wlt_console_error("Config: kernel path is empty");
        return WLT_BOOT_ERR_PARSE;
    }

    if (config->verify_signature && config->signature_path[0] == '\0')
    {
        wlt_console_error("Config: signature path is empty but verify=yes");
        return WLT_BOOT_ERR_PARSE;
    }

    return WLT_BOOT_OK;
}
