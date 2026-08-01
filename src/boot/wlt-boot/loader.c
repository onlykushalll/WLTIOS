/**
 * @file loader.c
 * @brief seL4 ELF loader implementation
 * @module wlt-boot/loader
 * @purpose Loads seL4 ELF binary into memory at correct addresses
 * @dependencies: loader.h, memory.h, console.h, boot.h
 * @tested_by: tests/boot/loader_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md, docs/adr/02-sel4-as-base.md
 *
 * UEFI file I/O uses:
 *   1. LocateProtocol(gEfiSimpleFileSystemProtocolGuid) → get SimpleFileSystem
 *   2. SimpleFileSystem->OpenVolume() → get Root file handle
 *   3. Root->Open(path) → get file handle
 *   4. File->GetInfo(FileSize) → get file size
 *   5. AllocatePool(size) → allocate buffer
 *   6. File->Read(buffer, size) → read file
 *   7. File->Close() → close file
 */

#include "loader.h"
#ifndef CHAR16
#define CHAR16 uint16_t
#endif
#include "memory.h"
#include "console.h"
#include <string.h>

/* ─── ELF Constants ──────────────────────────────────── */

#define EI_NIDENT  16

#define ELFMAG0    0x7f
#define ELFMAG1    'E'
#define ELFMAG2    'L'
#define ELFMAG3    'F'

#define ELFCLASS32  1
#define ELFCLASS64  2

#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_EXEC    2
#define ET_DYN     3

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4

#define EM_X86_64  62
#define EM_386     3

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

/* ─── UEFI Types (minimal) ──────────────────────────── */

typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

typedef struct {
    uint64_t Revision;
    void *ParentHandle;
    void *SystemTable;
    void *DeviceHandle;
    void *FilePath;
    void *Reserved;
    uint32_t LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    uint64_t ImageSize;
    void *CodeType;
    void *DataType;
    void *ImageErrorType;
    uint64_t ImageError;
    uint64_t ImageSize2;
    void *LoadOptions2;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct {
    uint64_t Revision;
    EFI_STATUS (*OpenVolume)(void *This, void **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct {
    uint64_t Revision;
    EFI_STATUS (*Open)(void *This, void **NewHandle, CHAR16 *FileName,
                       uint64_t OpenMode, uint64_t Attributes);
    EFI_STATUS (*Close)(void *This);
    EFI_STATUS (*Delete)(void *This);
    EFI_STATUS (*Read)(void *This, uint64_t *BufferSize, void *Buffer);
    EFI_STATUS (*Write)(void *This, uint64_t *BufferSize, void *Buffer);
    EFI_STATUS (*GetPosition)(void *This, uint64_t *Position);
    EFI_STATUS (*SetPosition)(void *This, uint64_t Position);
    EFI_STATUS (*GetInfo)(void *This, void *InformationType,
                          uint64_t *BufferSize, void *Buffer);
    EFI_STATUS (*SetInfo)(void *This, void *InformationType,
                          uint64_t BufferSize, void *Buffer);
    EFI_STATUS (*Flush)(void *This);
} EFI_FILE_PROTOCOL;

/* UEFI GUIDs (16 bytes each) */
typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} EFI_GUID;

/* gEfiSimpleFileSystemProtocolGuid */
static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x0964E5B2, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* gEfiLoadedImageProtocolGuid */
static const EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5B1B31A1, 0x9562, 0x11D2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* EFI_FILE_INFO GUID */
static const EFI_GUID gEfiFileInfoGuid = {
    0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

/* UEFI status codes */
#define EFI_SUCCESS             0
#define EFI_NOT_FOUND           EFIERR(14)
#define EFI_BUFFER_TOO_SMALL    EFIERR(5)
#define EFIERR(x)               (0x8000000000000000ULL | (x))

/* UEFI boot services (partial — only what we need) */
typedef struct {
    void *padding[11];  /* Offset to LocateProtocol */
    EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration,
                                  void **Interface);
    void *padding2[2];
    EFI_STATUS (*AllocatePool)(uint32_t PoolType, uint64_t Size, void **Buffer);
    EFI_STATUS (*FreePool)(void *Buffer);
} efi_boot_services_t;

/* EFI system table (partial) */
typedef struct {
    char dummy[56];
    void *BootServices;
} efi_system_table_t;

/* Module state */
static efi_system_table_t *g_st = NULL;
static efi_boot_services_t *g_bs = NULL;

/* ─── Internal: Convert ASCII path to UEFI CHAR16 ──── */

static void
ascii_to_char16(const char *ascii, CHAR16 *unicode, size_t max)
{
    size_t i;
    for (i = 0; ascii[i] != '\0' && i < max - 1; i++)
    {
        unicode[i] = (CHAR16)ascii[i];
    }
    unicode[i] = 0;
}

/* ─── Internal: Read file via UEFI SimpleFileSystem ── */

static wlt_boot_error_t
read_file(const char *path, uint8_t **data_out, size_t *size_out)
{
    EFI_STATUS status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_PROTOCOL *root = NULL;
    EFI_FILE_PROTOCOL *file = NULL;
    CHAR16 wpath[256];
    uint8_t *buffer = NULL;
    uint64_t file_size = 0;
    uint64_t buf_size;

    if (path == NULL || data_out == NULL || size_out == NULL)
        return WLT_BOOT_ERR_INVALID_ARG;

    /* Get boot services from system table */
    if (g_st == NULL || g_st->BootServices == NULL)
    {
        wlt_console_warn("No UEFI boot services available");
        return WLT_BOOT_ERR_IO;
    }
    g_bs = (efi_boot_services_t *)g_st->BootServices;

    /* Step 1: Locate SimpleFileSystem protocol */
    status = g_bs->LocateProtocol((EFI_GUID *)&gEfiSimpleFileSystemProtocolGuid,
                                   NULL, (void **)&fs);
    if (status != EFI_SUCCESS || fs == NULL)
    {
        wlt_console_error("Failed to locate SimpleFileSystem protocol");
        return WLT_BOOT_ERR_IO;
    }

    /* Step 2: Open volume (get root directory) */
    status = fs->OpenVolume(fs, (void **)&root);
    if (status != EFI_SUCCESS || root == NULL)
    {
        wlt_console_error("Failed to open volume");
        return WLT_BOOT_ERR_IO;
    }

    /* Step 3: Convert path to CHAR16 */
    ascii_to_char16(path, wpath, sizeof(wpath) / sizeof(CHAR16));

    /* Step 4: Open file */
    status = root->Open(root, (void **)&file, wpath, 1 /* READ */, 0);
    if (status != EFI_SUCCESS || file == NULL)
    {
        wlt_console_printf("Failed to open file: %s\n", path);
        root->Close(root);
        return WLT_BOOT_ERR_NOT_FOUND;
    }

    /* Step 5: Get file size via GetInfo */
    buf_size = 0;
    status = file->GetInfo(file, (void *)&gEfiFileInfoGuid, &buf_size, NULL);
    if (status == EFI_BUFFER_TOO_SMALL && buf_size > 0)
    {
        /* Allocate buffer for file info */
        uint8_t *info_buf;
        g_bs->AllocatePool(2 /* LoaderData */, buf_size, (void **)&info_buf);
        if (info_buf != NULL)
        {
            status = file->GetInfo(file, (void *)&gEfiFileInfoGuid,
                                    &buf_size, info_buf);
            if (status == EFI_SUCCESS)
            {
                /* File size is at offset 8 in EFI_FILE_INFO (after Size + Guid) */
                file_size = *(uint64_t *)(info_buf + 8);
            }
            g_bs->FreePool(info_buf);
        }
    }

    if (file_size == 0)
    {
        /* Fallback: try reading with 0 size to get required size */
        file_size = 1024 * 1024;  /* Assume max 1MB */
    }

    wlt_console_printf("File size: %llu bytes\n",
                        (unsigned long long)file_size);

    /* Step 6: Allocate buffer for file data */
    status = g_bs->AllocatePool(2 /* LoaderData */, file_size,
                                 (void **)&buffer);
    if (status != EFI_SUCCESS || buffer == NULL)
    {
        wlt_console_error("Failed to allocate file buffer");
        file->Close(file);
        root->Close(root);
        return WLT_BOOT_ERR_NOMEM;
    }

    /* Step 7: Read file */
    buf_size = file_size;
    status = file->Read(file, &buf_size, buffer);
    if (status != EFI_SUCCESS)
    {
        wlt_console_error("Failed to read file");
        g_bs->FreePool(buffer);
        file->Close(file);
        root->Close(root);
        return WLT_BOOT_ERR_IO;
    }

    /* Step 8: Close file + root */
    file->Close(file);
    root->Close(root);

    *data_out = buffer;
    *size_out = buf_size;

    wlt_console_printf("Read %llu bytes from %s\n",
                        (unsigned long long)buf_size, path);
    return WLT_BOOT_OK;
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
    kernel->data = elf_data;  /* C.1: store raw data for verification */

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

        if (load_address == 0)
        {
            load_address = phdr->p_paddr;
        }

        /* C.1: Real ELF segment loading via UEFI AllocatePages + memcpy.
         * For each PT_LOAD segment, allocate physical pages at p_paddr,
         * copy the file data (p_filesz bytes), and zero-fill BSS
         * (p_memsz - p_filesz bytes). This makes the kernel's code/data
         * available at the addresses the kernel expects. */
        if (g_bs != NULL && phdr->p_memsz > 0) {
            uint64_t pages = (phdr->p_memsz + 4095) / 4096;
            uint64_t phys = phdr->p_paddr;
            /* AllocatePages: AllocateAddress = 2, EfiLoaderData = 2 */
            typedef unsigned long long efi_status_t;
            efi_status_t (*allocate_pages)(uint32_t, uint32_t, uint64_t, uint64_t *);
            /* AllocatePages is at offset 24 in EFI_BOOT_SERVICES (after Hdr+RaiseTPL+RestoreTPL) */
            allocate_pages = *(efi_status_t (**)(uint32_t, uint32_t, uint64_t, uint64_t *))
                             ((char *)g_bs + 24);
            efi_status_t st = allocate_pages(2 /* AllocateAddress */, 2 /* EfiLoaderData */, pages, &phys);
            if (st == 0) {
                /* Copy file data */
                uint8_t *dst = (uint8_t *)phys;
                const uint8_t *src = elf_data + phdr->p_offset;
                for (uint64_t j = 0; j < phdr->p_filesz; j++)
                    dst[j] = src[j];
                /* Zero-fill BSS */
                for (uint64_t j = phdr->p_filesz; j < phdr->p_memsz; j++)
                    dst[j] = 0;
                wlt_console_printf("  Loaded %llu bytes at 0x%llx (BSS=%llu)\n",
                    (unsigned long long)phdr->p_filesz,
                    (unsigned long long)phys,
                    (unsigned long long)(phdr->p_memsz - phdr->p_filesz));
            } else {
                wlt_console_error("AllocatePages failed for segment");
            }
        }
    }

    kernel->load_address = load_address;

    wlt_console_printf("Kernel loaded: entry=0x%llx load=0x%llx size=%zu\n",
                        (unsigned long long)kernel->entry_point,
                        (unsigned long long)kernel->load_address,
                        kernel->size);

    /* Note: elf_data buffer should be freed after segments are copied */
    /* For now, keep it (will be freed when boot services are exited) */

    return WLT_BOOT_OK;
}

/* Set system table (called from efi_main) */
void
wlt_loader_set_system_table(void *system_table)
{
    g_st = (efi_system_table_t *)system_table;
}
