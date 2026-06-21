/**
 * @file loader.c
 * @brief seL4 ELF loader implementation
 * @module wlt-boot/loader
 * @purpose Loads seL4 ELF binary into memory at correct addresses
 * @dependencies: loader.h, memory.h, console.h, boot.h
 * @tested_by: tests/boot/loader_test.c
 * @spec: docs/specs/wlt-boot.md
 */

#include "loader.h"
#include "memory.h"
#include "console.h"
#include <string.h>

/* ─── ELF Constants ──────────────────────────────────── */

#define EI_NIDENT  16

/* ELF magic */
#define ELFMAG0    0x7f
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'

/* ELF class */
#define ELFCLASS32  1
#define ELFCLASS64  2

/* ELF data encoding */
#define ELFDATA2LSB 1  /* Little endian */
#define ELFDATA2MSB 2  /* Big endian */

/* ELF object type */
#define ET_EXEC    2   /* Executable */
#define ET_DYN     3   /* Shared object (PIE) */

/* Program header types */
#define PT_NULL    0
#define PT_LOAD    1   /* Loadable segment */
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4

/* ─── ELF64 Structures ──────────────────────────────── */

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_hdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

#define EM_X86_64  62

/* ─── Internal: Read file (UEFI stub) ───────────────── */

static wlt_boot_error_t
read_file(const char *path, uint8_t **data_out, size_t *size_out)
{
    /* TODO(wlt-boot): Implement via UEFI SimpleFileSystemProtocol */
    /* Steps: */
    /*   1. LocateProtocol(gEfiSimpleFileSystemProtocolGuid) */
    /*   2. OpenVolume() to get root file handle */
    /*   3. Open(path) to get file handle */
    /*   4. GetInfo(FileSize) to get file size */
    /*   5. AllocatePool(size) for buffer */
    /*   6. Read(buffer, size) */
    /*   7. Close file */

    /* For now, return error (no UEFI context) */
    (void)path;
    *data_out = NULL;
    *size_out = 0;
    wlt_console_warn("File reading not yet implemented (UEFI stub)");
    return WLT_BOOT_ERR_IO;
}

/* ─── Public API ─────────────────────────────────────── */

wlt_boot_error_t
wlt_loader_verify_elf(const uint8_t *data, size_t size)
{
    if (data == NULL || size < sizeof(elf64_hdr_t))
    {
        wlt_console_error("ELF: data too small");
        return WLT_BOOT_ERR_ELF;
    }

    const elf64_hdr_t *hdr = (const elf64_hdr_t *)data;

    /* Check magic */
    if (hdr->e_ident[0] != ELFMAG0 ||
        hdr->e_ident[1] != ELFMAG1 ||
        hdr->e_ident[2] != ELFMAG2 ||
        hdr->e_ident[3] != ELFMAG3)
    {
        wlt_console_error("ELF: invalid magic");
        return WLT_BOOT_ERR_ELF;
    }

    /* Check class (64-bit) */
    if (hdr->e_ident[4] != ELFCLASS64)
    {
        wlt_console_error("ELF: not 64-bit");
        return WLT_BOOT_ERR_ELF;
    }

    /* Check data encoding (little endian for x86) */
    if (hdr->e_ident[5] != ELFDATA2LSB)
    {
        wlt_console_error("ELF: not little endian");
        return WLT_BOOT_ERR_ELF;
    }

    /* Check machine type (x86-64) */
    if (hdr->e_machine != EM_X86_64)
    {
        wlt_console_printf("ELF: wrong machine (expected %d, got %d)\n",
                            EM_X86_64, hdr->e_machine);
        return WLT_BOOT_ERR_ELF;
    }

    /* Check type (executable or PIE) */
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN)
    {
        wlt_console_error("ELF: not executable");
        return WLT_BOOT_ERR_ELF;
    }

    /* Check program header count */
    if (hdr->e_phnum == 0)
    {
        wlt_console_error("ELF: no program headers");
        return WLT_BOOT_ERR_ELF;
    }

    /* Verify program header offset is within file */
    if (hdr->e_phoff + (uint64_t)hdr->e_phnum * hdr->e_phentsize > size)
    {
        wlt_console_error("ELF: program headers outside file");
        return WLT_BOOT_ERR_ELF;
    }

    wlt_console_info("ELF verification passed (64-bit x86-64 executable)");
    return WLT_BOOT_OK;
}

uint64_t
wlt_loader_get_entry(const uint8_t *elf_data)
{
    if (elf_data == NULL)
        return 0;

    const elf64_hdr_t *hdr = (const elf64_hdr_t *)elf_data;
    return hdr->e_entry;
}

wlt_boot_error_t
wlt_loader_load(const char *path, wlt_kernel_image_t *kernel)
{
    if (path == NULL || kernel == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    wlt_console_printf("Loading kernel: %s\n", path);

    /* Step 1: Read file */
    uint8_t *elf_data = NULL;
    size_t elf_size = 0;

    wlt_boot_error_t err = read_file(path, &elf_data, &elf_size);
    if (err != WLT_BOOT_OK)
    {
        wlt_console_error("Failed to read kernel file");
        return err;
    }

    wlt_console_printf("Kernel file: %zu bytes\n", elf_size);

    /* Step 2: Verify ELF header */
    err = wlt_loader_verify_elf(elf_data, elf_size);
    if (err != WLT_BOOT_OK)
    {
        return err;
    }

    const elf64_hdr_t *hdr = (const elf64_hdr_t *)elf_data;

    /* Step 3: Get entry point */
    kernel->entry_point = hdr->e_entry;
    kernel->size = elf_size;

    /* Step 4: Load PT_LOAD segments */
    uint64_t load_address = 0;

    for (uint16_t i = 0; i < hdr->e_phnum; i++)
    {
        const elf64_phdr_t *phdr = (const elf64_phdr_t *)
            (elf_data + hdr->e_phoff + i * hdr->e_phentsize);

        if (phdr->p_type != PT_LOAD)
            continue;

        wlt_console_printf("  Segment %u: vaddr=0x%llx paddr=0x%llx "
                            "filesz=%llu memsz=%llu\n",
                            i,
                            (unsigned long long)phdr->p_vaddr,
                            (unsigned long long)phdr->p_paddr,
                            (unsigned long long)phdr->p_filesz,
                            (unsigned long long)phdr->p_memsz);

        /* TODO(wlt-boot): Allocate physical memory at p_paddr */
        /* For now, just record the first load address */

        if (load_address == 0)
        {
            load_address = phdr->p_paddr;
        }

        /* TODO(wlt-boot): Copy segment data from file to p_paddr */
        /* memcpy((void*)phdr->p_paddr, elf_data + phdr->p_offset, phdr->p_filesz); */

        /* Zero-fill BSS (memsz > filesz) */
        /* if (phdr->p_memsz > phdr->p_filesz) */
        /* { */
        /*     memset((void*)(phdr->p_paddr + phdr->p_filesz), 0, */
        /*            phdr->p_memsz - phdr->p_filesz); */
        /* } */
    }

    kernel->load_address = load_address;

    wlt_console_printf("Kernel loaded: entry=0x%llx load=0x%llx size=%zu\n",
                        (unsigned long long)kernel->entry_point,
                        (unsigned long long)kernel->load_address,
                        kernel->size);

    /* TODO(wlt-boot): Free elf_data buffer */

    return WLT_BOOT_OK;
}
