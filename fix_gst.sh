#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# Make g_system_table non-static (extern-visible for memory.c)
sed -i 's/^static void \*g_system_table = NULL;/void *g_system_table = NULL;/' "$BOOTDIR/boot.c"
echo "boot.c g_system_table:"
grep "g_system_table" "$BOOTDIR/boot.c" | head -3

echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -8
echo ""
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "BUILD FAILED"
