/**
 * @file loader.h
 * @brief seL4 ELF loader for WLT-Boot
 * @module wlt-boot/loader
 * @purpose Loads seL4 ELF binary into memory at correct addresses
 * @dependencies: boot.h
 * @spec: docs/specs/wlt-boot.md
 */

#ifndef WLT_LOADER_H
#define WLT_LOADER_H

#include "boot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load kernel ELF from file
 * @param path        File path on ESP
 * @param kernel      Output kernel image info
 * @return WLT_BOOT_OK on success
 */
wlt_boot_error_t wlt_loader_load(const char *path,
                                  wlt_kernel_image_t *kernel);

/**
 * Verify ELF header
 * @param data  ELF file data
 * @param size  Data size
 * @return WLT_BOOT_OK if valid
 */
wlt_boot_error_t wlt_loader_verify_elf(const uint8_t *data, size_t size);

/**
 * Get entry point from ELF
 */
uint64_t wlt_loader_get_entry(const uint8_t *elf_data);

#ifdef __cplusplus
}
#endif

#endif /* WLT_LOADER_H */
