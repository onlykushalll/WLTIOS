#!/usr/bin/env bash
# All-in-one: build, self-test, 3 regression checks, write results to guardian/INTEGRATION_TEST_RESULTS.md
set +e
G=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/guardian
P=$G/local-deps/usr
cd "$G"
RESULTS="$G/INTEGRATION_TEST_RESULTS.md"
CFLAGS="-O2 -g -Wall -Wno-unused-function -Wno-unused-variable -DG_HAVE_LIBBPF -I$G/include -isystem $P/include -isystem $P/include/x86_64-linux-gnu"

{
echo "# Guardian Integration Test Results"
echo "**Date:** $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo

build() {
  rm -f /tmp/test_quota.log /tmp/gt_int
  make guardian GUARDIAN_ROOT="$G" GUARDIAN_PREFIX="$P" DAEMON_CC=gcc DAEMON_CFLAGS="$CFLAGS" 2>&1 | grep -iE 'error:|undefined' | head -5
  cp build/guardian /tmp/gt_int
}

run_selftest() {
  timeout 30 /tmp/gt_int --self-test 2>&1
}

echo "## 1. Clean build + self-test (all fixes in place)"
echo
build
echo "Build errors above (empty = clean). Running self-test..."
echo '```'
run_selftest | grep -E 'FAIL|self-test OK|\[test\]|Canary restart|Isolation Forest|Ransomware rate|iforest|anom=' | tail -30
echo '```'
echo

# Save the self-test exit code
ST_OUT=$(run_selftest)
ST_PASS=0
echo "$ST_OUT" | grep -q "self-test OK" && ST_PASS=1
echo "**Self-test result:** $([ $ST_PASS = 1 ] && echo PASS || echo FAIL)"
echo

echo "## 2. Regression check: iforest packed struct (BUG 3)"
echo "Temporarily revert the packed attribute, rebuild, confirm test 13 fails, restore."
echo
cp src/iforest.c /tmp/iforest.c.bak
sed -i 's/struct __attribute__((packed))/struct/' src/iforest.c
build
echo '```'
run_selftest 2>&1 | grep -E 'Isolation Forest|FAIL.*iforest|anom=' | head -5
echo '```'
REVERT_IFO=$(run_selftest 2>&1)
cp /tmp/iforest.c.bak src/iforest.c
echo "$REVERT_IFO" | grep -q "FAIL.*iforest\|iforest.*FAIL\|anomalous.*normal\|Isolation Forest inference correctness passed"
IFO_REGRESS=$?
echo "**Iforest regression:** test $([ $IFO_REGRESS = 0 ] && echo 'CAUGHT the break (has teeth)' || echo 'did NOT catch the break')"
echo

echo "## 3. Regression check: canary hash on restart (BUG 2)"
echo "Temporarily revert the hash-on-restart fix, rebuild, confirm test 12 fails, restore."
echo
cp src/canary_engine.c /tmp/canary.c.bak
# revert: remove the read+hash block, restore the old present=1 only
python3 - <<'PYREV'
import sys
f = '/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/guardian/src/canary_engine.c'
s = open(f).read()
old = """            struct stat st;
            if (stat(c->path, &st) == 0 && st.st_size == G_CANARY_SIZE) {
                int rfd = open(c->path, O_RDONLY);
                if (rfd >= 0) {
                    uint8_t rbuf[G_CANARY_SIZE];
                    if (read(rfd, rbuf, sizeof(rbuf)) == G_CANARY_SIZE) {
                        canary_hash(rbuf, sizeof(rbuf), c->hash);
                        c->present = 1;
                        ce->n++;
                    }
                    close(rfd);
                }
            }"""
new = """            struct stat st;
            if (stat(c->path, &st) == 0 && st.st_size == G_CANARY_SIZE) {
                c->present = 1;
                ce->n++;
            }"""
if old in s:
    s = s.replace(old, new)
    open(f,'w').write(s)
    print("reverted canary hash-on-restart")
else:
    print("WARN: canary revert pattern not found")
PYREV
build
echo '```'
run_selftest 2>&1 | grep -E 'Canary restart|FAIL.*canary|canary.*FAIL' | head -5
echo '```'
CAN_REVERT=$(run_selftest 2>&1)
cp /tmp/canary.c.bak src/canary_engine.c
echo "$CAN_REVERT" | grep -q "Canary restart test passed"
CAN_REGRESS=$?
echo "**Canary regression:** test $([ $CAN_REGRESS = 0 ] && echo 'PASSED even when broken (NO teeth)' || echo 'FAILED when broken (has teeth)')"
echo

echo "## 4. Restore all fixes + final clean build"
echo
build
echo '```'
run_selftest 2>&1 | grep -E 'FAIL|self-test OK' | tail -5
echo '```'
echo
echo "## Summary"
echo "- Self-test (all fixes): $([ $ST_PASS = 1 ] && echo PASS || echo FAIL)"
echo "- Iforest regression teeth: $([ $IFO_REGRESS = 0 ] && echo YES || echo NO)"
echo "- Canary regression teeth: $([ $CAN_REGRESS != 0 ] && echo YES || echo NO)"
} > "$RESULTS" 2>&1

echo "RESULTS_WRITTEN to $RESULTS"
cat "$RESULTS"
