#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

echo "=== Fix boot.c: replace TODO with real ExitBootServices + kernel jump ==="
# Use python with universal newlines
python3 << 'PYEOF'
import os

# Fix boot.c
with open(os.path.join(os.environ.get('BOOTDIR', ''), 'boot.c'), 'r', newline='') as f:
    content = f.read()

# Detect line ending
le = '\r\n' if '\r\n' in content else '\n'

old_block = (
    '    /* 6. Jump to kernel */' + le +
    '    wlt_console_info("Booting seL4...");' + le +
    le +
    '    /* TODO(wlt-boot): Transfer control to kernel.entry_point */' + le +
    le +
    '    /* Should never reach here */' + le +
    '    wlt_console_error("Kernel returned (should not happen)");' + le +
    '    return WLT_BOOT_ERR_IO;'
)

new_block = (
    '    /* 6. ExitBootServices + jump to kernel (C.1: real UEFI handoff) */' + le +
    '    wlt_console_info("Booting seL4...");' + le +
    le +
    '    /* ExitBootServices retry loop (UEFI spec 7.2): map key can change' + le +
    '     * between GetMemoryMap and ExitBootServices if a timer fires. */' + le +
    '    {' + le +
    '        /* BootServices is at offset 96 in EFI_SYSTEM_TABLE. */' + le +
    '        void *bs = *(void **)((char *)g_system_table + 96);' + le +
    '        if (bs == NULL) {' + le +
    '            wlt_console_error("No boot services");' + le +
    '            return WLT_BOOT_ERR_IO;' + le +
    '        }' + le +
    '        /* ExitBootServices at offset 208, GetMemoryMap at offset 32. */' + le +
    '        typedef unsigned long long efi_status_t;' + le +
    '        efi_status_t (*ebs)(void *, unsigned long long) =' + le +
    '            *(efi_status_t (**)(void *, unsigned long long))((char *)bs + 208);' + le +
    '        efi_status_t (*gmm)(unsigned long long *, void *,' + le +
    '                             unsigned long long *, unsigned long long *,' + le +
    '                             unsigned int *) =' + le +
    '            *(efi_status_t (**)(unsigned long long *, void *,' + le +
    '                                unsigned long long *, unsigned long long *,' + le +
    '                                unsigned int *))((char *)bs + 32);' + le +
    '        uint8_t map_buf[16384];' + le +
    '        unsigned long long msz = sizeof(map_buf), mkey = 0, dsz = 0;' + le +
    '        unsigned int dver = 0;' + le +
    '        efi_status_t est = 0;' + le +
    '        int i;' + le +
    '        for (i = 0; i < 8; i++) {' + le +
    '            msz = sizeof(map_buf);' + le +
    '            est = gmm(&msz, map_buf, &mkey, &dsz, &dver);' + le +
    '            if (est != 0) continue;' + le +
    '            est = ebs(image_handle, mkey);' + le +
    '            if (est == 0) break;' + le +
    '        }' + le +
    '        if (est != 0) {' + le +
    '            wlt_console_error("ExitBootServices failed");' + le +
    '            return WLT_BOOT_ERR_IO;' + le +
    '        }' + le +
    '    }' + le +
    '    /* Post-EBS: no more BootServices. Clean CPU state + jump. */' + le +
    '    __asm__ __volatile__("cli");' + le +
    '    __asm__ __volatile__("cld");' + le +
    '    /* Jump to kernel.entry_point (segments loaded at p_paddr by loader.c). */' + le +
    '    {' + le +
    '        typedef void (*kfn)(void);' + le +
    '        kfn entry = (kfn)(unsigned long long)kernel.entry_point;' + le +
    '        entry();' + le +
    '    }' + le +
    '    /* Should never reach here */' + le +
    '    wlt_console_error("Kernel returned (should not happen)");' + le +
    '    return WLT_BOOT_ERR_IO;'
)

if old_block in content:
    content = content.replace(old_block, new_block)
    with open(os.path.join(os.environ.get('BOOTDIR', ''), 'boot.c'), 'w', newline='') as f:
        f.write(content)
    print('boot.c: ExitBootServices + kernel jump implemented.')
else:
    print('boot.c: TODO block not found with exact match. Trying simpler approach...')
    # Simpler: just replace the TODO line
    if 'TODO(wlt-boot): Transfer control to kernel.entry_point' in content:
        content = content.replace(
            '    /* TODO(wlt-boot): Transfer control to kernel.entry_point */',
            '    /* C.1: Real UEFI handoff - ExitBootServices + jump to entry_point.\n'
            '     * BootServices at offset 96 in SystemTable. ExitBootServices at\n'
            '     * offset 208, GetMemoryMap at offset 32 in BootServices. */\n'
            '    {\n'
            '        void *bs = *(void **)((char *)g_system_table + 96);\n'
            '        if (bs) {\n'
            '            typedef unsigned long long efi_status_t;\n'
            '            efi_status_t (*ebs)(void *, unsigned long long) =\n'
            '                *(efi_status_t (**)(void *, unsigned long long))((char *)bs + 208);\n'
            '            efi_status_t (*gmm)(unsigned long long *, void *,\n'
            '                                 unsigned long long *, unsigned long long *,\n'
            '                                 unsigned int *) =\n'
            '                *(efi_status_t (**)(unsigned long long *, void *,\n'
            '                                    unsigned long long *, unsigned long long *,\n'
            '                                    unsigned int *))((char *)bs + 32);\n'
            '            uint8_t mb[16384]; unsigned long long sz=sizeof(mb),mk=0,ds=0; unsigned int dv=0;\n'
            '            efi_status_t s=0; int a;\n'
            '            for (a=0;a<8;a++) { sz=sizeof(mb); s=gmm(&sz,mb,&mk,&ds,&dv); if(s) continue; s=ebs(image_handle,mk); if(!s) break; }\n'
            '        }\n'
            '    }\n'
            '    __asm__ __volatile__("cli");\n'
            '    __asm__ __volatile__("cld");\n'
            '    { typedef void (*kfn)(void); kfn e=(kfn)(unsigned long long)kernel.entry_point; e(); }'
        )
        with open(os.path.join(os.environ.get('BOOTDIR', ''), 'boot.c'), 'w', newline='') as f:
            f.write(content)
        print('boot.c: Fixed via simpler approach.')
    else:
        print('boot.c: Could not find TODO.')

PYEOF

echo ""
echo "=== Fix memory.c: g_bs setter ==="
python3 << 'PYEOF2'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'memory.c')
with open(fpath, 'r', newline='') as f:
    content = f.read()
le = '\r\n' if '\r\n' in content else '\n'
old = ('    /* TODO(wlt-boot): Get boot services table from UEFI system table */' + le +
       '    /* g_bs = g_system_table->BootServices; */' + le +
       '    /* For now, this is a stub - the actual UEFI call needs the system table */')
new = ('    /* C.1: Get boot services from system table (offset 96 in EFI_SYSTEM_TABLE). */' + le +
       '    if (g_bs == NULL) {' + le +
       '        extern void *g_system_table;' + le +
       '        if (g_system_table != NULL)' + le +
       '            g_bs = *(void **)((char *)g_system_table + 96);' + le +
       '    }')
if old in content:
    content = content.replace(old, new)
    with open(fpath, 'w', newline='') as f:
        f.write(content)
    print('memory.c: g_bs setter added.')
elif 'TODO(wlt-boot): Get boot services' in content:
    content = content.replace(
        '    /* TODO(wlt-boot): Get boot services table from UEFI system table */\r\n    /* g_bs = g_system_table->BootServices; */\r\n    /* For now, this is a stub - the actual UEFI call needs the system table */',
        new.replace(le, '\r\n')
    )
    with open(fpath, 'w', newline='') as f:
        f.write(content)
    print('memory.c: Fixed via CRLF approach.')
else:
    print('memory.c: TODO not found.')
PYEOF2

echo ""
echo "=== VERIFY ==="
grep -c "ExitBootServices\|ebs\|kernel.entry_point" "$BOOTDIR/boot.c"
grep -c "g_system_table.*96\|g_bs.*96" "$BOOTDIR/memory.c"
echo "[DONE]"
