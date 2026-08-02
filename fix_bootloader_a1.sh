#!/bin/bash
# A.1-A.3: Rewrite boot.c, loader.c, memory.c to use real UEFI typed structs.
# Fixes: wrong offsets, missing ExitBootServices success check, fixed buffer.
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# 1. Install the vendored efi.h
cp /tmp/efi_vendored.h "$BOOTDIR/include/efi.h"
echo "Installed vendored efi.h ($(wc -l < "$BOOTDIR/include/efi.h") lines)"

# 2. Rewrite boot.c handoff section (A.1 + A.3)
python3 << 'PYEOF'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'boot.c')
with open(fpath, 'r', newline='') as f:
    content = f.read()

# Add efi.h include after the other includes
old_include = '#include <stdio.h>'
new_include = '#include <stdio.h>\n#include "efi.h"'
if old_include in content and '"efi.h"' not in content:
    content = content.replace(old_include, new_include, 1)

# Replace the entire handoff block with typed struct access + error check
old_handoff = '''    /* 6. Jump to kernel */
    wlt_console_info("Booting seL4...");
    /* C.1: Real UEFI handoff - ExitBootServices + jump to entry_point.
     * BootServices at offset 96 in SystemTable. ExitBootServices at
     * offset 208, GetMemoryMap at offset 32 in BootServices. */
    {
        void *bs = *(void **)((char *)g_system_table + 96);
        if (bs) {
            typedef unsigned long long efi_status_t;
            efi_status_t (*ebs)(void *, unsigned long long) =
                *(efi_status_t (**)(void *, unsigned long long))((char *)bs + 208);
            efi_status_t (*gmm)(unsigned long long *, void *,
                                 unsigned long long *, unsigned long long *,
                                 unsigned int *) =
                *(efi_status_t (**)(unsigned long long *, void *,
                                    unsigned long long *, unsigned long long *,
                                    unsigned int *))((char *)bs + 32);
            uint8_t mb[16384]; unsigned long long sz=sizeof(mb),mk=0,ds=0; unsigned int dv=0;
            efi_status_t s=0; int a;
            for (a=0;a<8;a++) { sz=sizeof(mb); s=gmm(&sz,mb,&mk,&ds,&dv); if(s) continue; s=ebs(image_handle,mk); if(!s) break; }
        }
    }
    __asm__ __volatile__("cli");
    __asm__ __volatile__("cld");
    { typedef void (*kfn)(void); kfn e=(kfn)(unsigned long long)kernel.entry_point; e(); }'''

new_handoff = '''    /* 6. ExitBootServices + jump to kernel (A.1-A.3: real UEFI handoff)
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
    return WLT_BOOT_ERR_IO;'''

if old_handoff in content:
    content = content.replace(old_handoff, new_handoff)
    with open(fpath, 'w', newline='') as f:
        f.write(content)
    print('boot.c: handoff rewritten with typed structs + EBS check.')
else:
    print('boot.c: old handoff block not found. Checking for partial state...')
    # Maybe already partially fixed — check if efi.h is included
    if '"efi.h"' in content:
        print('boot.c: already includes efi.h — may be partially fixed.')
    else:
        print('boot.c: ERROR - cannot find handoff block to replace.')
PYEOF

# 3. Rewrite loader.c to use real EFI_BOOT_SERVICES struct (A.1)
python3 << 'PYEOF2'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'loader.c')
with open(fpath, 'r', newline='') as f:
    content = f.read()

# Add efi.h include
if '"efi.h"' not in content:
    content = content.replace('#include "memory.h"', '#include "memory.h"\n#include "efi.h"', 1)

# Replace the fake efi_boot_services_t struct + g_bs usage with real typed access.
# The old code uses g_bs->LocateProtocol with a wrong-offset struct.
# Replace the struct definition with a comment pointing to efi.h.
old_struct = '''/* UEFI boot services (partial — only what we need) */
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
} efi_system_table_t;'''

new_struct = '''/* A.1: Real UEFI types from vendored efi.h (gnu-efi verified).
 * Previously used hand-recalled struct layouts with wrong offsets —
 * LocateProtocol was at offset 88 (should be 320), AllocatePool at
 * offset 112 (should be 64). Now uses the real EFI_BOOT_SERVICES. */

/* Old shims removed — use EFI_SYSTEM_TABLE / EFI_BOOT_SERVICES from efi.h */'''

if old_struct in content:
    content = content.replace(old_struct, new_struct)
    print('loader.c: fake struct replaced with efi.h reference.')
else:
    print('loader.c: old struct not found (may already be fixed).')

# Fix the g_st/g_bs types
content = content.replace(
    'static efi_system_table_t *g_st = NULL;\nstatic efi_boot_services_t *g_bs = NULL;',
    'static EFI_SYSTEM_TABLE *g_st = NULL;\nstatic EFI_BOOT_SERVICES *g_bs = NULL;'
)

# Fix wlt_loader_set_system_table
content = content.replace(
    'g_st = (efi_system_table_t *)system_table;',
    'g_st = (EFI_SYSTEM_TABLE *)system_table;'
)

# Fix the BootServices access
content = content.replace(
    'g_bs = (efi_boot_services_t *)g_st->BootServices;',
    'g_bs = g_st->BootServices;'
)

# Fix LocateProtocol call (old code used wrong cast, now use real struct)
# The old call: g_bs->LocateProtocol((EFI_GUID *)&gEfiSimpleFileSystemProtocolGuid, NULL, (void **)&fs);
# This should now work with the real struct since LocateProtocol is at the right offset.
# But the old EFI_GUID definition might conflict. Remove old local EFI_GUID.
old_guid = '''/* UEFI GUIDs (16 bytes each) */
typedef struct {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} EFI_GUID;'''
new_guid = '/* EFI_GUID defined in efi.h */'
if old_guid in content:
    content = content.replace(old_guid, new_guid)
    print('loader.c: local EFI_GUID removed (use efi.h).')

# Remove duplicate status code definitions (now in efi.h)
old_status = '''/* UEFI status codes */
#define EFI_SUCCESS             0
#define EFI_NOT_FOUND           EFIERR(14)
#define EFI_BUFFER_TOO_SMALL    EFIERR(5)
#define EFIERR(x)               (0x8000000000000000ULL | (x))'''
new_status = '/* UEFI status codes defined in efi.h */'
if old_status in content:
    content = content.replace(old_status, new_status)

with open(fpath, 'w', newline='') as f:
    f.write(content)
print('loader.c: rewritten with real EFI types.')
PYEOF2

# 4. Rewrite memory.c to use real EFI_BOOT_SERVICES struct (A.1)
python3 << 'PYEOF3'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'memory.c')
with open(fpath, 'r', newline='') as f:
    content = f.read()

# Add efi.h include
if '"efi.h"' not in content:
    content = content.replace('#include "console.h"', '#include "console.h"\n#include "efi.h"', 1)

# Replace the fake efi_boot_services_t struct
old_struct = '''/* UEFI Boot Services table (partial — only what we need) */
typedef struct {
    /* ... many fields before this ... */
    void *padding[22];  /* Offset to GetMemoryMap */
    int64_t (*GetMemoryMap)(uint64_t *MemoryMapSize, void *MemoryMap,
                            uint64_t *MapKey, uint64_t *DescriptorSize,
                            uint32_t *DescriptorVersion);
    void *padding2[1];  /* After GetMemoryMap */
    int64_t (*AllocatePool)(uint32_t PoolType, uint64_t Size, void **Buffer);
    int64_t (*FreePool)(void *Buffer);
} efi_boot_services_t;'''
new_struct = '/* A.1: Real EFI_BOOT_SERVICES from vendored efi.h. Old fake struct removed. */'
if old_struct in content:
    content = content.replace(old_struct, new_struct)
    print('memory.c: fake struct replaced.')
else:
    print('memory.c: old struct not found.')

# Fix g_bs type
content = content.replace(
    'static efi_boot_services_t *g_bs = NULL;',
    'static EFI_BOOT_SERVICES *g_bs = NULL;'
)

# Fix g_bs setter (the C.1 fix used raw offset 96 — now use typed struct)
old_setter = '''    /* C.1: Get boot services from system table (offset 96 in EFI_SYSTEM_TABLE) */
    if (g_bs == NULL) {
        extern void *g_system_table;
        if (g_system_table != NULL)
            g_bs = *(void **)((char *)g_system_table + 96);
    }'''
new_setter = '''    /* A.1: Get boot services via real typed struct (no magic offsets) */
    if (g_bs == NULL) {
        extern void *g_system_table;
        if (g_system_table != NULL) {
            EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE *)g_system_table;
            g_bs = st->BootServices;
        }
    }'''
if old_setter in content:
    content = content.replace(old_setter, new_setter)
    print('memory.c: g_bs setter uses typed struct.')
else:
    # Try the original TODO version
    old_todo = '''    /* TODO(wlt-boot): Get boot services table from UEFI system table */
    /* g_bs = g_system_table->BootServices; */
    /* For now, this is a stub - the actual UEFI call needs the system table */'''
    if old_todo in content:
        content = content.replace(old_todo, new_setter)
        print('memory.c: g_bs setter fixed from TODO.')
    else:
        print('memory.c: g_bs setter not found.')

with open(fpath, 'w', newline='') as f:
    f.write(content)
print('memory.c: rewritten with real EFI types.')
PYEOF3

echo ""
echo "=== BUILD TEST ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -15
echo ""
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "BUILD FAILED"
