/**
 * @file memory.h
 * @brief UEFI memory map handling for WLT-Boot
 * @module wlt-boot/memory
 * @purpose Retrieves and parses UEFI memory map for seL4
 * @dependencies: boot.h
 * @spec: docs/specs/wlt-boot.md
 */

#ifndef WLT_MEMORY_H
#define WLT_MEMORY_H

#include "boot.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retrieve UEFI memory map
 * @param map  Output memory map
 * @return WLT_BOOT_OK on success
 */
wlt_boot_error_t wlt_memory_get_map(wlt_memory_map_t *map);

/**
 * Free memory map
 */
void wlt_memory_free_map(wlt_memory_map_t *map);

/**
 * Find a conventional memory region of at least 'size' bytes
 * @param map    Memory map
 * @param size   Required size
 * @param align  Alignment (power of 2)
 * @return Region address, or 0 if not found
 */
uint64_t wlt_memory_find_region(const wlt_memory_map_t *map,
                                 uint64_t size,
                                 uint64_t align);

/**
 * Print memory map (for debugging)
 */
void wlt_memory_print(const wlt_memory_map_t *map);

#ifdef __cplusplus
}
#endif

#endif /* WLT_MEMORY_H */
