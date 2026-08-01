#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
MONO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/packages/monocypher

cp "$MONO/monocypher.h" "$BOOTDIR/"
sed -i 's|-Ipackages/monocypher|-I.|' "$BOOTDIR/Makefile"

echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -15
echo ""
echo "=== OUTPUT ==="
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "wlt-boot.efi not found"
