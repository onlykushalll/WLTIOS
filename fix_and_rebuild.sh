#!/bin/bash
# Fix rollback.c: move HMAC_HEX_LEN outside #ifdef HAVE_SODIUM.
# Also compile wlt-defender files with actual Makefile flags.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "=== 1. Read rollback.c around the #ifdef boundary ==="
sed -n '17,45p' "$REPO/wlt-update/rollback.c" 2>/dev/null

echo ""
echo "=== 2. Find where #ifdef HAVE_SODIUM ends ==="
grep -n "#ifdef\|#else\|#endif\|HAVE_SODIUM" "$REPO/wlt-update/rollback.c" 2>/dev/null

echo ""
echo "=== 3. Fix: move HMAC_HEX_LEN before the #ifdef ==="
# The fix: add HMAC_HEX_LEN define BEFORE the #ifdef HAVE_SODIUM block
# so it's always available regardless of sodium availability.
python3 << 'PYEOF'
import os
fpath = os.path.join(os.environ.get('REPO',''), 'wlt-update', 'rollback.c')
with open(fpath, 'r') as f:
    content = f.read()

# Check if HMAC_HEX_LEN is already outside the ifdef
# Find the #ifdef HAVE_SODIUM line
ifdef_line = content.find('#ifdef HAVE_SODIUM')
if ifdef_line == -1:
    print("ERROR: #ifdef HAVE_SODIUM not found")
else:
    # Find the HMAC_HEX_LEN define
    hmac_define = '#define HMAC_HEX_LEN 64'
    hmac_pos = content.find(hmac_define)
    if hmac_pos == -1:
        print("ERROR: HMAC_HEX_LEN define not found")
    elif hmac_pos < ifdef_line:
        print("HMAC_HEX_LEN already before #ifdef — nothing to do")
    else:
        # Remove the old define (inside #ifdef)
        content = content.replace(hmac_define + '\n', '', 1)
        # Add it before the #ifdef
        content = content[:ifdef_line] + '/* HMAC-SHA256 hex length: always defined (used by load/store\n' \
            ' * even when sodium is not compiled in, for buffer sizing). */\n' \
            + hmac_define + '\n\n' + content[ifdef_line:]
        with open(fpath, 'w') as f:
            f.write(content)
        print("Fixed: moved HMAC_HEX_LEN before #ifdef HAVE_SODIUM")
PYEOF

echo ""
echo "=== 4. Recompile rollback.c ==="
cd "$REPO/wlt-update"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC -c rollback.c -o /tmp/rollback.test3.o 2>&1
echo "rollback.c exit: $?"

echo ""
echo "=== 5. Compile wlt-defender 4 files with actual Makefile CFLAGS ==="
cd "$REPO/wlt-defender"
CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable -DG_HAVE_LIBBPF -Iinclude -Ilocal-deps/usr/include -Ilocal-deps/usr/include/x86_64-linux-gnu"
echo "--- anomaly_engine.c ---"
gcc $CFLAGS -c src/anomaly_engine.c -o /tmp/anomaly.test3.o 2>&1
echo "exit: $?"
echo "--- ebpf_engine.c ---"
gcc $CFLAGS -c src/ebpf_engine.c -o /tmp/ebpf.test3.o 2>&1
echo "exit: $?"
echo "--- forensic_log.c ---"
gcc $CFLAGS -c src/forensic_log.c -o /tmp/forensic.test3.o 2>&1
echo "exit: $?"
echo "--- net_engine.c ---"
gcc $CFLAGS -c src/net_engine.c -o /tmp/net_engine.test3.o 2>&1
echo "exit: $?"

echo ""
echo "=== 6. SUMMARY ==="
echo "rollback.c:       $(test -f /tmp/rollback.test3.o && echo 'OK (compiles clean with -Werror)' || echo 'FAIL')"
echo "anomaly_engine.c: $(test -f /tmp/anomaly.test3.o && echo 'OK (compiles with Makefile CFLAGS)' || echo 'FAIL')"
echo "ebpf_engine.c:    $(test -f /tmp/ebpf.test3.o && echo 'OK' || echo 'FAIL')"
echo "forensic_log.c:   $(test -f /tmp/forensic.test3.o && echo 'OK' || echo 'FAIL')"
echo "net_engine.c:     $(test -f /tmp/net_engine.test3.o && echo 'OK' || echo 'FAIL')"
