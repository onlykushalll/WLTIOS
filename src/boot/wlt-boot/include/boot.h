/**
 * @file boot.h
 * @brief Public interface for WLT-Boot (UEFI bootloader)
 * @module wlt-boot
 * @purpose Loads seL4 into memory, verifies signature, transfers control
 * @dependencies: efi.h, memory.h, loader.h, verify.h, config.h, console.h
 * @tested_by: tests/boot/boot_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md, docs/adr/02-sel4-as-base.md
 */

#ifndef WLT_BOOT_H
#define WLT_BOOT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Error Codes ────────────────────────────────────── */

typedef enum {
    WLT_BOOT_OK = 0,
    WLT_BOOT_ERR_INVALID_ARG = -1,
    WLT_BOOT_ERR_NOT_FOUND = -2,
    WLT_BOOT_ERR_IO = -3,
    WLT_BOOT_ERR_NOMEM = -4,
    WLT_BOOT_ERR_SIGNATURE = -5,
    WLT_BOOT_ERR_PARSE = -6,
    WLT_BOOT_ERR_ELF = -7,
    WLT_BOOT_ERR_TIMEOUT = -8,
} wlt_boot_error_t;

/* ─── Constants ──────────────────────────────────────── */

#define WLT_BOOT_VERSION "1.0.0"
#define WLT_BOOT_MAX_PATH 256
#define WLT_BOOT_MAX_CONFIG 4096
#define WLT_BOOT_TIMEOUT_MS 3000

/* ─── Types ──────────────────────────────────────────── */

typedef struct {
    uint64_t address;
    uint64_t size;
    uint32_t type;  /* EFI memory type */
} wlt_memory_region_t;

typedef struct {
    wlt_memory_region_t *regions;
    size_t region_count;
    uint64_t total_conventional;
    uint64_t total_reserved;
} wlt_memory_map_t;

typedef struct {
    char path[WLT_BOOT_MAX_PATH];
    uint64_t load_address;
    uint64_t entry_point;
    uint64_t size;
} wlt_kernel_image_t;

typedef struct {
    char kernel_path[WLT_BOOT_MAX_PATH];
    char signature_path[WLT_BOOT_MAX_PATH];
    char cmdline[256];
    bool verify_signature;
    uint32_t timeout_ms;
} wlt_boot_config_t;

/* ─── Public API ─────────────────────────────────────── */

/**
 * Boot entry point (called by UEFI firmware)
 * @param image_handle  UEFI image handle
 * @param system_table  UEFI system table
 * @return UEFI status
 */
int64_t efi_main(void *image_handle, void *system_table);

/**
 * Get human-readable error string
 */
const char *wlt_boot_error_str(wlt_boot_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_BOOT_H */
