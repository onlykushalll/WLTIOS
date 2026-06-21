/**
 * @file memory.c
 * @brief UEFI memory map handling implementation
 * @module wlt-boot/memory
 * @purpose Retrieves UEFI memory map and finds conventional regions for seL4
 * @dependencies: memory.h, console.h, boot.h
 * @tested_by: tests/boot/memory_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md
 */

#include "memory.h"
#include "console.h"
#include <string.h>

/* UEFI memory types (from UEFI Specification 2.8) */
#define EFI_RESERVED_MEMORY_TYPE     0
#define EFI_LOADER_CODE              1
#define EFI_LOADER_DATA              2
#define EFI_BOOT_SERVICES_CODE       3
#define EFI_BOOT_SERVICES_DATA       4
#define EFI_RUNTIME_SERVICES_CODE    5
#define EFI_RUNTIME_SERVICES_DATA    6
#define EFI_CONVENTIONAL_MEMORY      7
#define EFI_UNUSABLE_MEMORY          8
#define EFI_ACPI_RECLAIM_MEMORY      9
#define EFI_ACPI_MEMORY_NVS          10
#define EFI_MEMORY_MAPPED_IO         11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE                 13
#define EFI_PERSISTENT_MEMORY        14

/* UEFI memory descriptor structure */
typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

/* UEFI Boot Services table (partial — only what we need) */
typedef struct {
    /* ... many fields before this ... */
    void *padding[22];  /* Offset to GetMemoryMap */
    int64_t (*GetMemoryMap)(uint64_t *MemoryMapSize, void *MemoryMap,
                            uint64_t *MapKey, uint64_t *DescriptorSize,
                            uint32_t *DescriptorVersion);
    void *padding2[1];  /* After GetMemoryMap */
    int64_t (*AllocatePool)(uint32_t PoolType, uint64_t Size, void **Buffer);
    int64_t (*FreePool)(void *Buffer);
} efi_boot_services_t;

/* Module state */
static efi_boot_services_t *g_bs = NULL;
static uint8_t *g_map_buffer = NULL;
static uint64_t g_map_size = 0;
static uint64_t g_desc_size = 0;

/* ─── Internal: Get type name string ────────────────── */

static const char *
memory_type_str(uint32_t type)
{
    switch (type)
    {
        case EFI_CONVENTIONAL_MEMORY:      return "Conventional";
        case EFI_RESERVED_MEMORY_TYPE:     return "Reserved";
        case EFI_LOADER_CODE:              return "LoaderCode";
        case EFI_LOADER_DATA:              return "LoaderData";
        case EFI_BOOT_SERVICES_CODE:       return "BootServicesCode";
        case EFI_BOOT_SERVICES_DATA:       return "BootServicesData";
        case EFI_RUNTIME_SERVICES_CODE:    return "RuntimeServicesCode";
        case EFI_RUNTIME_SERVICES_DATA:    return "RuntimeServicesData";
        case EFI_UNUSABLE_MEMORY:          return "Unusable";
        case EFI_ACPI_RECLAIM_MEMORY:      return "ACPIReclaim";
        case EFI_ACPI_MEMORY_NVS:          return "ACPIMemoryNVS";
        case EFI_MEMORY_MAPPED_IO:         return "MMIO";
        case EFI_MEMORY_MAPPED_IO_PORT_SPACE: return "MMIOPort";
        case EFI_PAL_CODE:                 return "PALCode";
        case EFI_PERSISTENT_MEMORY:        return "Persistent";
        default:                           return "Unknown";
    }
}

/* ─── Public API ─────────────────────────────────────── */

wlt_boot_error_t
wlt_memory_get_map(wlt_memory_map_t *map)
{
    if (map == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    /* TODO(wlt-boot): Get boot services table from UEFI system table */
    /* g_bs = g_system_table->BootServices; */
    /* For now, this is a stub — the actual UEFI call needs the system table */

    if (g_bs == NULL)
    {
        /* No UEFI context — return empty map */
        map->regions = NULL;
        map->region_count = 0;
        map->total_conventional = 0;
        map->total_reserved = 0;
        wlt_console_warn("Memory map: no UEFI context (stub)");
        return WLT_BOOT_OK;
    }

    /* Step 1: Get map size */
    uint64_t map_size = 0;
    uint64_t map_key = 0;
    uint64_t desc_size = 0;
    uint32_t desc_version = 0;

    g_bs->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_version);

    if (map_size == 0)
    {
        wlt_console_error("GetMemoryMap returned size=0");
        return WLT_BOOT_ERR_IO;
    }

    /* Add extra space (map may grow after AllocatePool) */
    map_size += desc_size * 2;

    /* Step 2: Allocate buffer for map */
    void *buffer = NULL;
    g_bs->AllocatePool(EFI_LOADER_DATA, map_size, &buffer);
    if (buffer == NULL)
    {
        wlt_console_error("Failed to allocate memory map buffer");
        return WLT_BOOT_ERR_NOMEM;
    }

    /* Step 3: Get actual map */
    int64_t status = g_bs->GetMemoryMap(&map_size, buffer, &map_key,
                                         &desc_size, &desc_version);
    if (status != 0)
    {
        wlt_console_error("GetMemoryMap failed");
        g_bs->FreePool(buffer);
        return WLT_BOOT_ERR_IO;
    }

    /* Step 4: Parse descriptors */
    uint64_t count = map_size / desc_size;
    if (count > 256)
        count = 256;  /* Limit to prevent overflow */

    map->regions = (wlt_memory_region_t *)buffer;
    map->region_count = 0;
    map->total_conventional = 0;
    map->total_reserved = 0;

    for (uint64_t i = 0; i < count; i++)
    {
        efi_memory_descriptor_t *desc =
            (efi_memory_descriptor_t *)((uint8_t *)buffer + i * desc_size);

        map->regions[map->region_count].address = desc->physical_start;
        map->regions[map->region_count].size = desc->number_of_pages * 4096;
        map->regions[map->region_count].type = desc->type;

        if (desc->type == EFI_CONVENTIONAL_MEMORY)
        {
            map->total_conventional += map->regions[map->region_count].size;
        }
        else
        {
            map->total_reserved += map->regions[map->region_count].size;
        }

        map->region_count++;
    }

    g_map_buffer = (uint8_t *)buffer;
    g_map_size = map_size;
    g_desc_size = desc_size;

    wlt_console_printf("Memory map: %zu regions, %llu MB conventional, %llu MB reserved\n",
                        map->region_count,
                        (unsigned long long)(map->total_conventional / (1024 * 1024)),
                        (unsigned long long)(map->total_reserved / (1024 * 1024)));

    return WLT_BOOT_OK;
}

void
wlt_memory_free_map(wlt_memory_map_t *map)
{
    if (map == NULL)
        return;

    /* TODO(wlt-boot): Free via UEFI FreePool */
    /* g_bs->FreePool(map->regions); */
    map->regions = NULL;
    map->region_count = 0;
}

uint64_t
wlt_memory_find_region(const wlt_memory_map_t *map,
                        uint64_t size,
                        uint64_t align)
{
    if (map == NULL || size == 0)
        return 0;

    if (align == 0)
        align = 4096;  /* Default: page alignment */

    for (size_t i = 0; i < map->region_count; i++)
    {
        const wlt_memory_region_t *r = &map->regions[i];

        /* Only use conventional memory */
        if (r->type != EFI_CONVENTIONAL_MEMORY)
            continue;

        /* Check if region is large enough */
        if (r->size < size)
            continue;

        /* Calculate aligned address */
        uint64_t aligned = (r->address + align - 1) & ~(align - 1);

        /* Check if aligned address + size fits in region */
        if (aligned + size <= r->address + r->size)
        {
            return aligned;
        }
    }

    /* No suitable region found */
    return 0;
}

void
wlt_memory_print(const wlt_memory_map_t *map)
{
    if (map == NULL)
    {
        wlt_console_puts("  (null map)\r\n");
        return;
    }

    wlt_console_printf("  Regions: %zu\n", map->region_count);
    wlt_console_printf("  Conventional: %llu MB\n",
                        (unsigned long long)(map->total_conventional / (1024 * 1024)));
    wlt_console_printf("  Reserved: %llu MB\n",
                        (unsigned long long)(map->total_reserved / (1024 * 1024)));

    /* Print first 10 regions (for debugging) */
    size_t max_print = map->region_count < 10 ? map->region_count : 10;
    for (size_t i = 0; i < max_print; i++)
    {
        const wlt_memory_region_t *r = &map->regions[i];
        wlt_console_printf("  [%zu] %s: 0x%llx - 0x%llx (%llu KB)\n",
                            i,
                            memory_type_str(r->type),
                            (unsigned long long)r->address,
                            (unsigned long long)(r->address + r->size),
                            (unsigned long long)(r->size / 1024));
    }

    if (map->region_count > 10)
    {
        wlt_console_printf("  ... (%zu more regions)\n",
                            map->region_count - 10);
    }
}
