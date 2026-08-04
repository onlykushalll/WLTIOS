#!/bin/bash
# Fix rollback.c: HMAC_HEX_LEN is inside #ifdef HAVE_SODIUM but used outside.
# Check if HAVE_SODIUM should be defined, or move the macro.
set +e
export REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "=== 1. Check if wlt-update Makefile defines HAVE_SODIUM ==="
grep -n "HAVE_SODIUM\|sodium\|SODIUM" "$REPO/wlt-update/Makefile" 2>/dev/null | head -10

echo ""
echo "=== 2. Check if sodium is installed ==="
pkg-config --libs libsodium 2>/dev/null || echo "libsodium not found via pkg-config"
ls /usr/lib/x86_64-linux-gnu/libsodium* 2>/dev/null | head -3
dpkg -l 2>/dev/null | grep sodium | head -3

echo ""
echo "=== 3. Check if HAVE_SODIUM is in any config header ==="
grep -rn "HAVE_SODIUM" "$REPO/wlt-update/" 2>/dev/null | head -10

echo ""
echo "=== 4. Read the full #ifdef HAVE_SODIUM block (lines 19-75) ==="
sed -n '19,75p' "$REPO/wlt-update/rollback.c" 2>/dev/null

echo ""
echo "=== 5. Read wlt_rollback_load (lines 76-115) ==="
sed -n '76,115p' "$REPO/wlt-update/rollback.c" 2>/dev/null
