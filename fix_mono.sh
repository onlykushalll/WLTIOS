#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
MONO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/packages/monocypher

# Add monocypher include path to Makefile
sed -i "s|-Iinclude|-Iinclude -I${MONO}|" "$BOOTDIR/Makefile"
echo "Makefile CFLAGS:"
grep "^CFLAGS" "$BOOTDIR/Makefile"

echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -15
echo ""
echo "=== OUTPUT ==="
ls -la wlt-boot.efi 2>/dev/null || echo "wlt-boot.efi not found"
file wlt-boot.efi 2>/dev/null || true
