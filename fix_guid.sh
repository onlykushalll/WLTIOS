#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# Add gEfiFileInfoGuid to efi.h (it was removed from loader.c but loader.c uses it)
python3 << 'PYEOF'
import os
fpath = os.path.join(os.environ.get('BOOTDIR', ''), 'include/efi.h')
with open(fpath, 'r') as f:
    content = f.read()
if 'gEfiFileInfoGuid' not in content:
    old = 'static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {'
    new = '''/* gEfiFileInfoGuid (for EFI_FILE_PROTOCOL->GetInfo) */
static const EFI_GUID gEfiFileInfoGuid = {
    0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}
};

static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {'''
    content = content.replace(old, new)
    with open(fpath, 'w') as f:
        f.write(content)
    print('efi.h: gEfiFileInfoGuid added.')
else:
    print('efi.h: already has gEfiFileInfoGuid.')
PYEOF

echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -15
echo ""
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "BUILD FAILED"
