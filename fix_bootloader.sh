#!/bin/bash
# Fix all bootloader issues identified by Section C analysis.
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

echo "=== 1. Fix memory.c typo: regionsap->region_count] -> regions[map->region_count] ==="
sed -i 's/map->regionsap->region_count\]/map->regions[map->region_count]/g' "$BOOTDIR/memory.c"
grep -c 'regions\[map->region_count\]' "$BOOTDIR/memory.c"
echo "Fixed."

echo ""
echo "=== 2. Fix loader.c GUID overflow: 0x0964E5B22 -> 0x0964E5B2 ==="
sed -i 's/0x0964E5B22/0x0964E5B2/g' "$BOOTDIR/loader.c"
grep 'SimpleFileSystemProtocolGuid' "$BOOTDIR/loader.c" | head -1
echo "Fixed."

echo ""
echo "=== 3. Fix loader.c: implement ELF segment loading ==="
# Replace the TODO block with real AllocatePages + memcpy + BSS zero-fill
python3 -c "
import re
with open('$BOOTDIR/loader.c', 'r') as f:
    content = f.read()

old = '''        /* TODO(wlt-boot): Allocate physical memory at p_paddr via UEFI */
        /* and copy segment data + zero-fill BSS */
        /* This requires UEFI AllocatePages + memcpy */'''

new = '''        /* C.1: Real ELF segment loading via UEFI AllocatePages + memcpy.
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
                wlt_console_printf(\"  Loaded %llu bytes at 0x%llx (BSS=%llu)\\n\",
                    (unsigned long long)phdr->p_filesz,
                    (unsigned long long)phys,
                    (unsigned long long)(phdr->p_memsz - phdr->p_filesz));
            } else {
                wlt_console_error(\"AllocatePages failed for segment\");
            }
        }'''

if old in content:
    content = content.replace(old, new)
    with open('$BOOTDIR/loader.c', 'w') as f:
        f.write(content)
    print('ELF segment loading implemented.')
else:
    print('TODO block not found (may already be fixed).')
"

echo ""
echo "=== 4. Fix boot.c: implement ExitBootServices + kernel jump ==="
python3 -c "
with open('$BOOTDIR/boot.c', 'r') as f:
    content = f.read()

old = '''    /* 6. Jump to kernel */
    wlt_console_info(\"Booting seL4...\");

    /* TODO(wlt-boot): Transfer control to kernel.entry_point */

    /* Should never reach here */
    wlt_console_error(\"Kernel returned (should not happen)\");
    return WLT_BOOT_ERR_IO;'''

new = '''    /* 6. ExitBootServices + jump to kernel (C.1: real UEFI handoff) */
    wlt_console_info(\"Booting seL4...\");

    /* ExitBootServices retry loop (UEFI spec section 7.2): the map key
     * can change between GetMemoryMap and ExitBootServices if a timer
     * event fires. Must re-query and retry until success. */
    {
        /* Get the real EFI_BOOT_SERVICES pointer from the system table.
         * In the real UEFI SystemTable, BootServices is at offset 96
         * (Hdr 24 + FirmwareVendor 8 + FirmwareRevision 4 + pad 4 +
         * ConsoleInHandle 8 + ConIn 8 + ConsoleOutHandle 8 + ConOut 8 +
         * StdErrHandle 8 + StdErr 8 = 96). */
        void *bs = *(void **)((char *)g_system_table + 96);
        if (bs == NULL) {
            wlt_console_error(\"No boot services - cannot hand off\");
            return WLT_BOOT_ERR_IO;
        }

        /* ExitBootServices is at offset 208 in EFI_BOOT_SERVICES.
         * We use a raw function pointer to avoid struct layout issues. */
        typedef unsigned long long efi_status_t;
        efi_status_t (*exit_boot_services)(void *, unsigned long long);
        exit_boot_services = *(efi_status_t (**)(void *, unsigned long long))
                              ((char *)bs + 208);

        /* GetMemoryMap is at offset 32 in EFI_BOOT_SERVICES. */
        efi_status_t (*get_memory_map)(unsigned long long *, void *,
                                        unsigned long long *,
                                        unsigned long long *,
                                        unsigned int *);
        get_memory_map = *(efi_status_t (**)(unsigned long long *, void *,
                                              unsigned long long *,
                                              unsigned long long *,
                                              unsigned int *))
                         ((char *)bs + 32);

        uint8_t map_buf[4096 * 4];
        unsigned long long map_size = sizeof(map_buf);
        unsigned long long map_key = 0, desc_size = 0;
        unsigned int desc_ver = 0;
        efi_status_t est;
        int attempts;
        for (attempts = 0; attempts < 8; attempts++) {
            map_size = sizeof(map_buf);
            est = get_memory_map(&map_size, map_buf, &map_key, &desc_size, &desc_ver);
            if (est != 0) {
                wlt_console_warn(\"GetMemoryMap failed, retrying\");
                continue;
            }
            est = exit_boot_services(image_handle, map_key);
            if (est == 0) {
                wlt_console_info(\"ExitBootServices succeeded\");
                break;
            }
            wlt_console_warn(\"ExitBootServices retry (map key stale)\");
        }
        if (est != 0) {
            wlt_console_error(\"ExitBootServices failed after retries\");
            return WLT_BOOT_ERR_IO;
        }
    }

    /* Post-ExitBootServices: only RuntimeServices are valid. No more
     * console OutputString (ConOut belongs to BootServices). We are now
     * the only code running. Tear down to clean CPU state and jump. */
    __asm__ __volatile__(\"cli\");
    __asm__ __volatile__(\"cld\");

    /* Jump to kernel.entry_point. UEFI leaves us at CPL0 with paging on
     * and an identity map covering low memory. The kernel ELF segments
     * were loaded at their p_paddr (see loader.c), so a direct
     * rip-transfer is correct for a flat 64-bit seL4-style entry. */
    {
        typedef void (*kernel_entry_fn)(void);
        kernel_entry_fn entry = (kernel_entry_fn)(unsigned long long)kernel.entry_point;
        entry();  /* does not return */
    }

    /* Should never reach here */
    wlt_console_error(\"Kernel returned (should not happen)\");
    return WLT_BOOT_ERR_IO;'''

if old in content:
    content = content.replace(old, new)
    with open('$BOOTDIR/boot.c', 'w') as f:
        f.write(content)
    print('ExitBootServices + kernel jump implemented.')
else:
    print('Handoff TODO not found (may already be fixed).')
"

echo ""
echo "=== 5. Fix memory.c: add g_bs setter ==="
# Add wlt_memory_set_system_table function
python3 -c "
with open('$BOOTDIR/memory.c', 'r') as f:
    content = f.read()

old = '''    /* TODO(wlt-boot): Get boot services table from UEFI system table */
    /* g_bs = g_system_table->BootServices; */
    /* For now, this is a stub - the actual UEFI call needs the system table */'''

new = '''    /* C.1: Get boot services from system table (offset 96 in EFI_SYSTEM_TABLE) */
    if (g_bs == NULL) {
        extern void *g_system_table;
        if (g_system_table != NULL) {
            g_bs = *(void **)((char *)g_system_table + 96);
        }
    }'''

if old in content:
    content = content.replace(old, new)
    with open('$BOOTDIR/memory.c', 'w') as f:
        f.write(content)
    print('g_bs setter added.')
else:
    print('g_bs TODO not found (may already be fixed).')
"

echo ""
echo "=== VERIFY ALL FIXES ==="
echo "memory.c typo: $(grep -c 'regions\[map->region_count\]' "$BOOTDIR/memory.c") occurrences (should be 4)"
echo "GUID: $(grep '0x0964E5B2' "$BOOTDIR/loader.c" | head -1 | grep -c '0x0964E5B2[^2]')"
echo "Segment loading: $(grep -c 'Real ELF segment loading' "$BOOTDIR/loader.c")"
echo "ExitBootServices: $(grep -c 'ExitBootServices' "$BOOTDIR/boot.c")"
echo "kernel jump: $(grep -c 'kernel_entry_fn' "$BOOTDIR/boot.c")"
echo "g_bs setter: $(grep -c 'g_system_table.*96' "$BOOTDIR/memory.c")"
echo "[DONE]"
