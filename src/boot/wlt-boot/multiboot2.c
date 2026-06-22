/**
 * @file multiboot2.c
 * @brief Multiboot2 protocol implementation for WLT-Boot
 * @module wlt-boot/multiboot2
 * @purpose Provides multiboot2 header so seL4 can be loaded by GRUB or direct boot
 * @dependencies: boot.h
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md
 *
 * The multiboot2 header tells the bootloader (GRUB or UEFI) where to find
 * the kernel entry point and what memory layout to use.
 */

#include "boot.h"

/* Multiboot2 magic number */
#define MULTIBOOT2_HEADER_MAGIC  0xe85250d6
#define MULTIBOOT2_ARCHITECTURE_I386  0
#define MULTIBOOT2_ARCHITECTURE_MIPS  4
#define MULTIBOOT2_HEADER_END       0
#define MULTIBOOT2_HEADER_TAG_SIZE  8

/* Multiboot2 header structure (placed at start of kernel image) */
typedef struct {
    uint32_t magic;
    uint32_t architecture;
    uint32_t header_length;
    uint32_t checksum;
} multiboot2_header_t;

/* Information request tag */
typedef struct {
    uint16_t type;
    uint16_t flags;
    uint32_t size;
    uint32_t mbi_tag_types[1];  /* Request basic memory info */
} multiboot2_info_req_t;

/* Entry address tag (where to jump after loading) */
typedef struct {
    uint16_t type;    /* = 3 */
    uint16_t flags;
    uint32_t size;
    uint32_t entry_addr;
} multiboot2_entry_addr_t;

/* End tag */
typedef struct {
    uint16_t type;    /* = 0 */
    uint16_t flags;
    uint32_t size;
} multiboot2_end_tag_t;

/**
 * Build the multiboot2 header in memory.
 * This is called by WLT-Boot before jumping to seL4.
 *
 * The header tells GRUB/QEMU:
 *   - This is a multiboot2 kernel
 *   - Architecture is i386 (x86)
 *   - Entry point address
 *   - What info we want (memory map, cmdline, etc.)
 */
wlt_boot_error_t
wlt_multiboot2_build(uint8_t *header_buf, size_t buf_size,
                      uint64_t entry_point, size_t *header_size)
{
    if (header_buf == NULL || buf_size < 64 || header_size == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    size_t offset = 0;

    /* 1. Main header */
    multiboot2_header_t *hdr = (multiboot2_header_t *)(header_buf + offset);
    hdr->magic = MULTIBOOT2_HEADER_MAGIC;
    hdr->architecture = MULTIBOOT2_ARCHITECTURE_I386;
    /* header_length will be set after we know the total size */
    hdr->checksum = 0;
    offset += sizeof(*hdr);

    /* 2. Information request tag (type=1) */
    multiboot2_info_req_t *info_req = (multiboot2_info_req_t *)(header_buf + offset);
    info_req->type = 1;  /* Information request */
    info_req->flags = 0;
    info_req->size = sizeof(*info_req);
    info_req->mbi_tag_types[0] = 4;  /* Request basic memory info */
    offset += info_req->size;

    /* Align to 8 bytes */
    while (offset % 8 != 0)
        offset++;

    /* 3. Entry address tag (type=3) */
    multiboot2_entry_addr_t *entry = (multiboot2_entry_addr_t *)(header_buf + offset);
    entry->type = 3;  /* Entry address */
    entry->flags = 0;
    entry->size = sizeof(*entry);
    entry->entry_addr = (uint32_t)entry_point;
    offset += entry->size;

    /* Align to 8 bytes */
    while (offset % 8 != 0)
        offset++;

    /* 4. End tag (type=0) */
    multiboot2_end_tag_t *end = (multiboot2_end_tag_t *)(header_buf + offset);
    end->type = 0;
    end->flags = 0;
    end->size = sizeof(*end);
    offset += end->size;

    /* 5. Set header_length + calculate checksum */
    hdr->header_length = (uint32_t)offset;
    /* Checksum: magic + arch + length + checksum = 0 (mod 2^32) */
    hdr->checksum = -(hdr->magic + hdr->architecture + hdr->header_length);

    *header_size = offset;

    return WLT_BOOT_OK;
}

/**
 * Parse multiboot2 boot information (provided by GRUB/loader after boot)
 * Extracts: memory map, command line, modules (initrd)
 */
wlt_boot_error_t
wlt_multiboot2_parse(const void *mbi_info,
                      wlt_memory_map_t *mem_map,
                      char *cmdline, size_t cmdline_max,
                      uint64_t *module_start, uint64_t *module_end)
{
    if (mbi_info == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    const uint8_t *ptr = (const uint8_t *)mbi_info;
    uint32_t total_size = *(const uint32_t *)ptr;
    (void)total_size;

    /* Skip the first 8 bytes (total_size + reserved) */
    ptr += 8;

    /* Iterate through tags */
    while (1)
    {
        uint16_t tag_type = *(const uint16_t *)ptr;
        uint16_t tag_flags = *(const uint16_t *)(ptr + 2);
        uint32_t tag_size = *(const uint32_t *)(ptr + 4);
        (void)tag_flags;

        if (tag_type == 0)  /* End tag */
            break;

        switch (tag_type)
        {
            case 1:  /* Boot command line */
                if (cmdline != NULL && cmdline_max > 0)
                {
                    const char *str = (const char *)(ptr + 8);
                    strncpy(cmdline, str, cmdline_max - 1);
                    cmdline[cmdline_max - 1] = '\0';
                }
                break;

            case 6:  /* Memory map */
                if (mem_map != NULL)
                {
                    uint32_t entry_size = *(const uint32_t *)(ptr + 8);
                    uint32_t entry_version = *(const uint32_t *)(ptr + 12);
                    (void)entry_version;

                    const uint8_t *entry_ptr = ptr + 16;
                    size_t count = 0;

                    while (entry_ptr < ptr + tag_size && count < 256)
                    {
                        uint64_t base_addr = *(const uint64_t *)entry_ptr;
                        uint64_t length = *(const uint64_t *)(entry_ptr + 8);
                        uint32_t type = *(const uint32_t *)(entry_ptr + 16);

                        if (count < WLT_BOOT_MAX_PATH)  /* reuse a max */
                        {
                            mem_map->regions[count].address = base_addr;
                            mem_map->regions[count].size = length;
                            mem_map->regions[count].type = type;
                            count++;
                        }

                        entry_ptr += entry_size;
                    }
                    mem_map->region_count = count;
                }
                break;

            case 3:  /* Modules (initrd) */
                if (module_start != NULL && module_end != NULL)
                {
                    *module_start = *(const uint64_t *)(ptr + 8);
                    *module_end = *(const uint64_t *)(ptr + 16);
                }
                break;
        }

        /* Advance to next tag (align to 8 bytes) */
        ptr += tag_size;
        while ((uintptr_t)ptr % 8 != 0)
            ptr++;
    }

    return WLT_BOOT_OK;
}
