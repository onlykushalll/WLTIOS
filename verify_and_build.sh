#!/bin/bash
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot
echo "=== VERIFY boot.c ==="
grep -n "ExitBootServices\|kernel.entry_point\|ebs(" "$BOOTDIR/boot.c" | head -10
echo ""
echo "=== VERIFY memory.c ==="
grep -n "g_system_table\|g_bs" "$BOOTDIR/memory.c" | head -10
echo ""
echo "=== GCC CHECK ==="
which x86_64-elf-gcc x86_64-w64-mingw32-gcc gcc 2>/dev/null
echo ""
echo "=== TRY BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -3
make 2>&1 | tail -20
