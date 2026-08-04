#!/bin/bash
# Compile individual files directly to verify the fixes.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace

echo "================================================================"
echo "A.2 — wlt-update: rollback.c (direct compile)"
echo "================================================================"
cd "$REPO/wlt-update"
echo "--- compile rollback.c with -Werror ---"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude -D_GNU_SOURCE -fPIC -c rollback.c -o /tmp/rollback.test.o 2>&1
echo "rollback.c exit: $?"

echo ""
echo "================================================================"
echo "A.3 — wlt-defender: 4 files (direct compile)"
echo "================================================================"
cd "$REPO/wlt-defender"

# Check the Makefile for CFLAGS
echo "--- Makefile CFLAGS ---"
grep -E "^CFLAGS|^CC|^INCLUDES" Makefile 2>/dev/null | head -10

echo ""
echo "--- compile anomaly_engine.c ---"
gcc -Wall -Wextra -Werror -O2 -Iinclude -Isrc -D_GNU_SOURCE -c src/anomaly_engine.c -o /tmp/anomaly.test.o 2>&1
echo "anomaly_engine.c exit: $?"

echo ""
echo "--- compile ebpf_engine.c ---"
gcc -Wall -Wextra -Werror -O2 -Iinclude -Isrc -D_GNU_SOURCE -c src/ebpf_engine.c -o /tmp/ebpf.test.o 2>&1
echo "ebpf_engine.c exit: $?"

echo ""
echo "--- compile forensic_log.c ---"
gcc -Wall -Wextra -Werror -O2 -Iinclude -Isrc -D_GNU_SOURCE -c src/forensic_log.c -o /tmp/forensic.test.o 2>&1
echo "forensic_log.c exit: $?"

echo ""
echo "--- compile net_engine.c ---"
gcc -Wall -Wextra -Werror -O2 -Iinclude -Isrc -D_GNU_SOURCE -c src/net_engine.c -o /tmp/net_engine.test.o 2>&1
echo "net_engine.c exit: $?"

echo ""
echo "================================================================"
echo "SUMMARY"
echo "================================================================"
echo "rollback.c:       $(test -f /tmp/rollback.test.o && echo 'OK' || echo 'FAIL')"
echo "anomaly_engine.c: $(test -f /tmp/anomaly.test.o && echo 'OK' || echo 'FAIL')"
echo "ebpf_engine.c:    $(test -f /tmp/ebpf.test.o && echo 'OK' || echo 'FAIL')"
echo "forensic_log.c:   $(test -f /tmp/forensic.test.o && echo 'OK' || echo 'FAIL')"
echo "net_engine.c:     $(test -f /tmp/net_engine.test.o && echo 'OK' || echo 'FAIL')"
