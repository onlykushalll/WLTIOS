#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
MEM="$BOOTDIR/memory.c"

# 1. Fix the regionsap typo (all occurrences)
sed -i 's/map->regionsap->region_count\]/map->regions[map->region_count]/g' "$MEM"
echo "Typo fix: $(grep -c 'regions\[map->region_count\]' "$MEM") occurrences"

# 2. Fix field names to real UEFI CamelCase
sed -i 's/desc->physical_start/desc->PhysicalStart/g' "$MEM"
sed -i 's/desc->number_of_pages/desc->NumberOfPages/g' "$MEM"
sed -i 's/desc->type/desc->Type/g' "$MEM"

echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -15
echo ""
ls -la wlt-boot.efi 2>/dev/null && file wlt-boot.efi 2>/dev/null || echo "BUILD FAILED"
