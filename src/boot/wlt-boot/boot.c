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

/* Forward declaration — defined in loader.c */
extern void wlt_loader_set_system_table(void *system_table);

int64_t
efi_main(void *image_handle, void *system_table)
{
 (void)image_handle;
    wlt_boot_config_t config;
    wlt_memory_map_t mem_map;
    wlt_kernel_image_t kernel;
    wlt_boot_error_t err;

    /* 1. Initialize console + pass system table to modules */
    wlt_console_init(system_table);
    wlt_loader_set_system_table(system_table);
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

    /* 5. Verify signature via real Ed25519 (monocypher, Section A fix) */
    if (config.verify_signature)
    {
        wlt_console_info("Verifying kernel signature (Ed25519)...");
        /* Load the signature file from the ESP */
        wlt_kernel_image_t sig_img;
        char sig_path[256];
        snprintf(sig_path, sizeof(sig_path), "%s.sig", config.kernel_path);
        err = wlt_loader_load(sig_path, &sig_img);
        if (err != WLT_BOOT_OK)
        {
            wlt_console_error("Failed to load signature file");
            return err;
        }
        /* Real Ed25519 verification via monocypher. Fail-closed.
         * The boot pubkey (32 bytes) is loaded from /boot/keys/boot-pubkey.bin
         * on the ESP. It is SEPARATE from the package signing key (trust boundary).
         * Signature is always 64 bytes (Ed25519 standard). */
        wlt_kernel_image_t pubkey_img;
        err = wlt_loader_load("/boot/keys/boot-pubkey.bin", &pubkey_img);
        if (err != WLT_BOOT_OK)
        {
            wlt_console_error("Failed to load boot public key");
            wlt_console_error("Expected at /boot/keys/boot-pubkey.bin (32 bytes)");
            return err;
        }
        if (pubkey_img.size < 32)
        {
            wlt_console_error("Boot public key too short (need 32 bytes)");
            return WLT_BOOT_ERR_INVALID_ARG;
        }
        err = wlt_verify_ed25519(kernel.data, kernel.size,
                                 sig_img.data,
                                 pubkey_img.data);
        if (err != WLT_BOOT_OK)
        {
            wlt_console_error("Signature INVALID - refusing to boot!");
            return err;
        }
        wlt_console_info("Signature verified (Ed25519 real check)");
    }
    else
    {
        wlt_console_warn("Signature verification DISABLED (debug mode)");
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
