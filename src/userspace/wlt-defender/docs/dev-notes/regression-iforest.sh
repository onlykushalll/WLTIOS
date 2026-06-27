#!/usr/bin/env bash
# Fix sha256 files + run iforest regression test
G=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/guardian
P=$G/local-deps/usr
cd "$G"

# Fix sha256 files
python3 <<'SHA'
import hashlib
for name, f in [('anomaly.pkl','ml/anomaly.pkl'), ('anomaly.iforest','ml/anomaly.iforest')]:
    h = hashlib.sha256(open(f,'rb').read()).hexdigest()
    open(f+'.sha256','w').write(h + '  ' + f + '\n')
    print(f'{name} sha256: {h}')
SHA

echo "=== IFOREST REGRESSION TEST ==="
echo "1. Build with packed (FIX in place) -> test should PASS"
cp src/iforest.c /tmp/iforest.good
make guardian GUARDIAN_ROOT="$G" GUARDIAN_PREFIX="$P" DAEMON_CC=gcc DAEMON_CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable -DG_HAVE_LIBBPF -I$G/include -isystem $P/include -isystem $P/include/x86_64-linux-gnu" 2>&1 | grep -c 'error:' 
rm -f /tmp/test_quota.log /tmp/gt_int
cp build/guardian /tmp/gt_int
ST_GOOD=$(timeout 30 /tmp/gt_int --self-test 2>&1)
echo "$ST_GOOD" | grep -E 'Isolation Forest|anom=|self-test OK' | tail -4
echo "$ST_GOOD" | grep -q "self-test OK" && echo "RESULT: PASS (fix in place)" || echo "RESULT: FAIL"

echo ""
echo "2. Revert packed (BREAK the fix) -> test should FAIL"
sed -i 's/struct __attribute__((packed))/struct/' src/iforest.c
make guardian GUARDIAN_ROOT="$G" GUARDIAN_PREFIX="$P" DAEMON_CC=gcc DAEMON_CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable -DG_HAVE_LIBBPF -I$G/include -isystem $P/include -isystem $P/include/x86_64-linux-gnu" 2>&1 | grep -c 'error:'
rm -f /tmp/test_quota.log /tmp/gt_int
cp build/guardian /tmp/gt_int
ST_BAD=$(timeout 30 /tmp/gt_int --self-test 2>&1)
echo "$ST_BAD" | grep -E 'Isolation Forest|anom=|FAIL.*iforest|self-test OK|Segmentation' | tail -4
echo "$ST_BAD" | grep -q "self-test OK" && echo "RESULT: test did NOT catch break (NO teeth)" || echo "RESULT: test CAUGHT the break (has teeth)"

echo ""
echo "3. Restore fix -> test should PASS again"
cp /tmp/iforest.good src/iforest.c
make guardian GUARDIAN_ROOT="$G" GUARDIAN_PREFIX="$P" DAEMON_CC=gcc DAEMON_CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable -DG_HAVE_LIBBPF -I$G/include -isystem $P/include -isystem $P/include/x86_64-linux-gnu" 2>&1 | grep -c 'error:'
rm -f /tmp/test_quota.log /tmp/gt_int
cp build/guardian /tmp/gt_int
timeout 30 /tmp/gt_int --self-test 2>&1 | grep -E 'self-test OK|FAIL' | tail -3
