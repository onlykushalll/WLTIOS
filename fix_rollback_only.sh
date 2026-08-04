#!/bin/bash
# Just fix rollback.c + recompile (no apt-get).
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace
FILE="$REPO/wlt-update/rollback.c"

echo "=== Before fix (lines 18-22) ==="
sed -n '18,22p' "$FILE"

# Insert HMAC_HEX_LEN before #ifdef HAVE_SODIUM (line 19)
sed -i '19i\/* HMAC-SHA256 hex length: always defined (used by wlt_rollback_load\n * for buffer sizing even when sodium is not compiled in). */\n#define HMAC_HEX_LEN 64\n' "$FILE"

echo ""
echo "=== After fix (lines 18-28) ==="
sed -n '18,28p' "$FILE"

echo ""
echo "=== Recompile rollback.c WITHOUT HAVE_SODIUM ==="
cd "$REPO/wlt-update"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC -c rollback.c -o /tmp/rollback.final.o 2>&1
echo "rollback.c (no sodium) exit: $?"

echo ""
echo "=== Recompile rollback.c WITH HAVE_SODIUM ==="
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -DHAVE_SODIUM -fPIC -c rollback.c -o /tmp/rollback.sodium.o 2>&1
echo "rollback.c (with sodium) exit: $?"
