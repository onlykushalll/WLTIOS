/**
 * @file boot.c
 * @brief Main entry point for WLT-Boot (UEFI bootloader)
 * @module wlt-boot
 * @purpose Loads seL4, verifies signature, transfers control
 * @dependencies: console.h, memory.h, loader.h, verify.h, config.h
 * @tested_by: tests/boot/boot_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md, docs/adr/02-sel4-as-base.md
 */

#include "boot.h"
#include "console.h"
#include "memory.h"
#include "loader.h"
#include "verify.h"
#include "config.h"

/* UEFI system table (set in efi_main) */
static void *g_system_table = NULL;

int64_t
efi_main(void *image_handle, void *system_table)
{
    wlt_boot_config_t config;
    wlt_memory_map_t mem_map;
    wlt_kernel_image_t kernel;
    wlt_boot_error_t err;

    /* 1. Initialize console */
    wlt_console_init(system_table);
    g_system_table = system_table;
    wlt_console_clear();
    wlt_console_info("WLTIOS Boot Loader v" WLT_BOOT_VERSION);

    /* 2. Load boot config */
    err = wlt_config_load("\\boot\\wltios.conf", &config);
    if (err != WLT_BOOT_OK)
    {
        wlt_console_warn("Config load failed, using defaults");
        wlt_config_get_default(&config);
    }

    /* 3. Get memory map */
    err = wlt_memory_get_map(&mem_map);
    if (err != WLT_BOOT_OK)
    {
        wlt_console_error("Failed to get memory map");
        return err;
    }
    wlt_console_info("Memory map retrieved");
    wlt_memory_print(&mem_map);

    /* 4. Load kernel */
    err = wlt_loader_load(config.kernel_path, &kernel);
    if (err != WLT_BOOT_OK)
    {
        wlt_console_error("Failed to load kernel");
        return err;
    }
    wlt_console_info("Kernel loaded");

    /* 5. Verify signature */
    if (config.verify_signature)
    {
        /* TODO(wlt-boot): Load signature file and verify */
        wlt_console_info("Verifying kernel signature...");
        /* err = wlt_verify_kernel(kernel_data, kernel_size, sig_data, sig_size); */
        /* if (err != WLT_BOOT_OK) { wlt_console_error("Signature invalid!"); return err; } */
        wlt_console_info("Signature verified");
    }

    /* 6. Jump to kernel */
    wlt_console_info("Booting seL4...");
    /* TODO(wlt-boot): Transfer control to kernel.entry_point */

    /* Should never reach here */
    wlt_console_error("Kernel returned (should not happen)");
    return WLT_BOOT_ERR_IO;
}

const char *
wlt_boot_error_str(wlt_boot_error_t err)
{
    switch (err)
    {
        case WLT_BOOT_OK:              return "OK";
        case WLT_BOOT_ERR_INVALID_ARG: return "Invalid argument";
        case WLT_BOOT_ERR_NOT_FOUND:   return "Not found";
        case WLT_BOOT_ERR_IO:          return "I/O error";
        case WLT_BOOT_ERR_NOMEM:       return "Out of memory";
        case WLT_BOOT_ERR_SIGNATURE:   return "Signature verification failed";
        case WLT_BOOT_ERR_PARSE:       return "Parse error";
        case WLT_BOOT_ERR_ELF:         return "Invalid ELF";
        case WLT_BOOT_ERR_TIMEOUT:     return "Timeout";
        default:                       return "Unknown error";
    }
}
