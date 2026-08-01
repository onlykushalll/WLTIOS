#!/bin/bash
set -e
BOOTDIR=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/boot/wlt-boot

# Fix boot.h: add stdbool.h
sed -i 's|#include <stddef.h>|#include <stddef.h>\n#include <stdbool.h>|' "$BOOTDIR/include/boot.h"

# Fix boot.c: add stdio.h for snprintf
sed -i 's|#include "config.h"|#include "config.h"\n#include <stdio.h>|' "$BOOTDIR/boot.c"

echo "Includes fixed."
echo ""
echo "=== BUILD ==="
cd "$BOOTDIR"
make clean 2>&1 | head -1
make 2>&1 | tail -25
