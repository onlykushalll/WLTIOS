#!/bin/bash
# Fix rollback.c: move HMAC_HEX_LEN before #ifdef HAVE_SODIUM.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace
FILE="$REPO/wlt-update/rollback.c"

echo "=== Before fix (lines 18-22) ==="
sed -n '18,22p' "$FILE"

# Insert HMAC_HEX_LEN before #ifdef HAVE_SODIUM (line 19)
# Use sed to insert before the #ifdef line
sed -i '19i\/* HMAC-SHA256 hex length: always defined (used by wlt_rollback_load\n * for buffer sizing even when sodium is not compiled in). */\n#define HMAC_HEX_LEN 64\n' "$FILE"

echo ""
echo "=== After fix (lines 18-28) ==="
sed -n '18,28p' "$FILE"

echo ""
echo "=== Recompile rollback.c ==="
cd "$REPO/wlt-update"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC -c rollback.c -o /tmp/rollback.final.o 2>&1
ROLLBACK_EXIT=$?
echo "rollback.c exit: $ROLLBACK_EXIT"

echo ""
echo "=== Also try with HAVE_SODIUM (install libsodium-dev) ==="
sudo apt-get install -y libsodium-dev 2>&1 | tail -3
echo "--- recompile with HAVE_SODIUM ---"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -DHAVE_SODIUM -fPIC -c rollback.c -o /tmp/rollback.sodium.o 2>&1
echo "rollback.c with HAVE_SODIUM exit: $?"

echo ""
echo "=== Full wlt-update build ==="
make clean 2>&1 | tail -2
make 2>&1
echo "wlt-update make exit: $?"
