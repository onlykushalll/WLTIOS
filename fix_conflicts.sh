#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# 1. Remove duplicate EFI_STATUS typedef from loader.c (efi.h has it)
sed -i '/^typedef unsigned long long EFI_STATUS;$/d' "$BOOTDIR/loader.c"

# 2. Remove duplicate gEfiSimpleFileSystemProtocolGuid from loader.c (efi.h has it)
# Also remove the gEfiLoadedImageProtocolGuid (unused, and uses old GUID type)
python3 << 'PYEOF'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'loader.c')
with open(fpath, 'r') as f:
    lines = f.readlines()
out = []
skip = 0
for line in lines:
    if skip > 0:
        # Skip until closing brace + semicolon
        if '};' in line:
            skip = 0
        continue
    if 'static const EFI_GUID gEfiSimpleFileSystemProtocolGuid' in line:
        skip = 1
        continue
    if 'static const EFI_GUID gEfiLoadedImageProtocolGuid' in line:
        skip = 1
        continue
    if 'static const EFI_GUID gEfiFileInfoGuid' in line:
        skip = 1
        continue
    out.append(line)
with open(fpath, 'w') as f:
    f.writelines(out)
print('loader.c: duplicate GUIDs removed.')
PYEOF

# 3. Fix memory.c g_bs setter — line 87 has old TODO comment + line 90 has partial fix
python3 << 'PYEOF2'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'memory.c')
with open(fpath, 'r') as f:
    content = f.read()

# Replace the old g_bs setter block (lines ~85-95)
old = '''    /* g_bs = g_system_table->BootServices; */
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
    }'''

new = '''    /* A.1: Get boot services via real typed struct (no magic offsets) */
    if (g_bs == NULL) {
        extern void *g_system_table;
        if (g_system_table != NULL) {
            EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE *)g_system_table;
            g_bs = st->BootServices;
        }
    }

    if (g_bs == NULL)
    {
        map->regions = NULL;
        map->region_count = 0;
        map->total_conventional = 0;
        map->total_reserved = 0;
        wlt_console_warn("Memory map: no UEFI context (stub)");
        return WLT_BOOT_OK;
    }'''

if old in content:
    content = content.replace(old, new)
    with open(fpath, 'w') as f:
        f.write(content)
    print('memory.c: g_bs setter fixed.')
else:
    print('memory.c: old setter not found, checking current state...')
    # Maybe it has CRLF or different formatting
    if 'g_system_table->BootServices' in content or 'g_system_table + 96' in content:
        print('memory.c: has some form of g_bs setter, may need manual check.')
    else:
        print('memory.c: NO g_bs setter found.')
PYEOF2

# Also fix EFI_STATUS return type in memory.c (was int64_t, should be EFI_STATUS)
sed -i 's/int64_t status = g_bs->GetMemoryMap/EFI_STATUS status = g_bs->GetMemoryMap/' "$BOOTDIR/memory.c"

# 4. Fix the EFI_MEMORY_DESCRIPTOR references — memory.c has its own local one
python3 << 'PYEOF3'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'memory.c')
with open(fpath, 'r') as f:
    content = f.read()
# Remove local efi_memory_descriptor_t typedef (efi.h has EFI_MEMORY_DESCRIPTOR)
old_desc = '''/* UEFI memory descriptor structure */
typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;'''
if old_desc in content:
    content = content.replace(old_desc, '/* EFI_MEMORY_DESCRIPTOR from efi.h */')
    # Replace all efi_memory_descriptor_t with EFI_MEMORY_DESCRIPTOR
    content = content.replace('efi_memory_descriptor_t', 'EFI_MEMORY_DESCRIPTOR')
    with open(fpath, 'w') as f:
        f.write(content)
    print('memory.c: local descriptor removed, using EFI_MEMORY_DESCRIPTOR.')
else:
    print('memory.c: local descriptor not found.')
PYEOF3

echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -20
echo ""
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "BUILD FAILED"
