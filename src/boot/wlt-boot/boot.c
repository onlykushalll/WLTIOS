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
#include <stdio.h>
#include "efi.h"

/* UEFI system table (set in efi_main) */
void *g_system_table = NULL;

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

    /* 6. ExitBootServices + jump to kernel (A.1-A.3: real UEFI handoff)
     *
     * A.1: Uses real UEFI typed structs from vendored efi.h (verified
     *      against gnu-efi via offsetof). No magic byte offsets.
     * A.2: Memory-map buffer is dynamically allocated via AllocatePool,
     *      sized to what the firmware reports (not a fixed stack buffer).
     * A.3: ExitBootServices return value is checked. If it fails after
     *      8 retries, we HALT — never jump to the kernel with boot
     *      services still active (undefined behavior).
     */
    wlt_console_info("Booting seL4...");

    EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE *)g_system_table;
    EFI_BOOT_SERVICES *bs = st->BootServices;
    if (bs == NULL) {
        wlt_console_error("No BootServices - cannot hand off");
        return WLT_BOOT_ERR_IO;
    }

    /* A.2: Dynamic memory-map buffer. GetMemoryMap tells us the needed
     * size; we AllocatePool exactly that much (plus slack for the
     * allocation itself changing the map). */
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR *map_buf = NULL;

    /* First call: get required size (buffer too small is expected) */
    bs->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);
    map_size += desc_size * 4;  /* slack for AllocatePool changing the map */

    EFI_STATUS status = bs->AllocatePool(EFI_LOADER_DATA, map_size, (void **)&map_buf);
    if (status != EFI_SUCCESS || map_buf == NULL) {
        wlt_console_error("Failed to allocate memory-map buffer");
        return WLT_BOOT_ERR_NOMEM;
    }

    /* A.3: ExitBootServices retry loop (UEFI spec 7.2). The map key
     * becomes stale if memory changes between GetMemoryMap and
     * ExitBootServices. Must retry until success. */
    EFI_STATUS ebs_status = (EFI_STATUS)0xFFFFFFFF;  /* sentinel: not-succeeded */
    int attempt;
    for (attempt = 0; attempt < 8; attempt++) {
        UINTN sz = map_size;
        status = bs->GetMemoryMap(&sz, map_buf, &map_key, &desc_size, &desc_version);
        if (status != EFI_SUCCESS) {
            wlt_console_warn("GetMemoryMap retry needed");
            continue;
        }
        ebs_status = bs->ExitBootServices(image_handle, map_key);
        if (ebs_status == EFI_SUCCESS) {
            wlt_console_info("ExitBootServices succeeded");
            break;
        }
        /* EFI_INVALID_PARAMETER = map key stale. Retry. */
        wlt_console_warn("ExitBootServices retry (map key stale)");
    }

    /* A.3: CRITICAL — check success before jumping. If ExitBootServices
     * never succeeded, boot services are still active. Jumping to the
     * kernel in this state is undefined/catastrophic. Halt instead. */
    if (ebs_status != EFI_SUCCESS) {
        wlt_console_error("ExitBootServices FAILED after 8 retries");
        wlt_console_error("Refusing to jump - boot services still active");
        /* Halt: do NOT jump to kernel with firmware still active. */
        __asm__ __volatile__("cli");
        for (;;) {
            __asm__ __volatile__("hlt");
        }
        return WLT_BOOT_ERR_IO;  /* unreachable, but keeps compiler happy */
    }

    /* Post-ExitBootServices: only RuntimeServices valid. No more ConOut
     * (belongs to BootServices). Clean CPU state + jump to kernel. */
    __asm__ __volatile__("cli");
    __asm__ __volatile__("cld");

    /* Jump to kernel.entry_point. ELF segments were loaded at their
     * p_paddr by loader.c. A direct rip-transfer is correct for a
     * flat 64-bit seL4-style entry. */
    typedef void (*kernel_entry_fn)(void);
    kernel_entry_fn entry = (kernel_entry_fn)(uintptr_t)kernel.entry_point;
    entry();  /* does not return */

    /* Should never reach here */
    wlt_console_error("Kernel returned (should not happen)");
    return WLT_BOOT_ERR_IO;

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
