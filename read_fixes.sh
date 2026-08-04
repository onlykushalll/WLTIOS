#!/bin/bash
# Read the files to understand the correct constants + actual Makefile CFLAGS.
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "=== 1. rollback.h (find HMAC constants) ==="
cat "$REPO/wlt-update/rollback.h" 2>/dev/null

echo ""
echo "=== 2. rollback.c lines 80-100 (the fix) ==="
sed -n '80,100p' "$REPO/wlt-update/rollback.c" 2>/dev/null

echo ""
echo "=== 3. grep for HMAC in rollback.c/h ==="
grep -n "HMAC\|hmac\|SHA256\|sha256\|HEX" "$REPO/wlt-update/rollback.c" "$REPO/wlt-update/rollback.h" 2>/dev/null | head -20

echo ""
echo "=== 4. wlt-defender Makefile CFLAGS ==="
grep -E "^CFLAGS|^CC|^INCLUDES|^DEFINES|Werror|Wall" "$REPO/wlt-defender/Makefile" 2>/dev/null | head -15

echo ""
echo "=== 5. wlt-defender Makefile (full, first 50 lines) ==="
head -50 "$REPO/wlt-defender/Makefile" 2>/dev/null

echo ""
echo "=== 6. grep for HMAC_SHA256_HEX_LEN or similar in wlt-update ==="
grep -rn "HMAC.*LEN\|SHA256.*HEX\|hmac.*hex" "$REPO/wlt-update/" 2>/dev/null | head -10
