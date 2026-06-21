/**
 * @file loader.c
 * @brief seL4 ELF loader implementation
 * @module wlt-boot/loader
 * @purpose Loads seL4 ELF binary into memory
 * @spec: docs/specs/wlt-boot.md
 */

#include "loader.h"
#include "console.h"
#include "memory.h"

wlt_boot_error_t
wlt_loader_load(const char *path, wlt_kernel_image_t *kernel)
{
    /* TODO(wlt-boot): Implement ELF loading */
    /* This requires: */
    /*   - Open file via UEFI SimpleFileSystemProtocol */
    /*   - Read ELF header */
    /*   - Verify ELF magic (0x7f 'E' 'L' 'F') */
    /*   - Parse program headers */
    /*   - For each PT_LOAD segment: */
    /*     - Allocate memory at p_paddr */
    /*     - Read segment data */
    /*     - Zero-fill BSS (memsz > filesz) */
    /*   - Set kernel->entry_point = e_entry */
    /*   - Set kernel->load_address, kernel->size */
    (void)path;
    (void)kernel;
    return WLT_BOOT_OK;
}

wlt_boot_error_t
wlt_loader_verify_elf(const uint8_t *data, size_t size)
{
    /* TODO(wlt-boot): Verify ELF magic + header */
    (void)data;
    (void)size;
    return WLT_BOOT_OK;
}

uint64_t
wlt_loader_get_entry(const uint8_t *elf_data)
{
    /* TODO(wlt-boot): Read entry point from ELF header */
    (void)elf_data;
    return 0;
}
