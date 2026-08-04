#!/bin/bash
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "=== 1. Full rollback.c (lines 1-50) ==="
sed -n '1,50p' "$REPO/wlt-update/rollback.c" 2>/dev/null

echo ""
echo "=== 2. Full wlt-defender Makefile CFLAGS section ==="
grep -A5 "DAEMON_CFLAGS\|CFLAGS" "$REPO/wlt-defender/Makefile" 2>/dev/null | head -20

echo ""
echo "=== 3. wlt-defender Makefile daemon build rule ==="
grep -A10 "guardian:" "$REPO/wlt-defender/Makefile" 2>/dev/null | head -15

echo ""
echo "=== 4. Check if rollback.c compiles with wlt-update's actual Makefile flags ==="
cd "$REPO/wlt-update"
# The Makefile uses: cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC
# But verify.c has a pre-existing unused-function error. Let's just compile rollback.c alone.
echo "--- Direct compile with same flags as Makefile ---"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC -c rollback.c -o /tmp/rollback.test2.o 2>&1
echo "exit: $?"

echo ""
echo "=== 5. wlt-defender: compile 4 files with actual Makefile CFLAGS ==="
cd "$REPO/wlt-defender"
DAEMON_CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable"
echo "--- anomaly_engine.c ---"
gcc $DAEMON_CFLAGS -Iinclude -Isrc -D_GNU_SOURCE -c src/anomaly_engine.c -o /tmp/anomaly.test2.o 2>&1
echo "exit: $?"
echo "--- ebpf_engine.c ---"
gcc $DAEMON_CFLAGS -Iinclude -Isrc -D_GNU_SOURCE -c src/ebpf_engine.c -o /tmp/ebpf.test2.o 2>&1
echo "exit: $?"
echo "--- forensic_log.c ---"
gcc $DAEMON_CFLAGS -Iinclude -Isrc -D_GNU_SOURCE -c src/forensic_log.c -o /tmp/forensic.test2.o 2>&1
echo "exit: $?"
echo "--- net_engine.c ---"
gcc $DAEMON_CFLAGS -Iinclude -Isrc -D_GNU_SOURCE -c src/net_engine.c -o /tmp/net_engine.test2.o 2>&1
echo "exit: $?"
