#!/bin/bash
# tests/integration/sel4_boot_test.sh
# Boot the seL4 image with KVM, wait 90 seconds, verify WLTIOS boots inside seL4.
# Reports PASS/FAIL for each expected serial line.

LOG=/tmp/wltios-sel4-boot-test.log
PASS=0
FAIL=0

echo "=== WLTIOS seL4 Boot Integration Test ==="

# Boot seL4 with KVM
timeout 90 qemu-system-i386 \
    -machine pc -cpu host -m 512M -enable-kvm \
    -kernel /home/kushal/wltios-build/build-minimal-32/images/kernel-ia32-pc99 \
    -initrd /home/kushal/wltios-build/build-minimal-32/images/capdl-loader-image-ia32-pc99 \
    -netdev user,id=net0 -device e1000,netdev=net0 \
    -nographic -no-reboot -serial mon:stdio \
    > $LOG 2>&1

echo ""
echo "--- Checks ---"

# Check 1: seL4 kernel boots (dropped to user space)
if grep -q "Booting all finished, dropped to user space" $LOG; then
    echo "[PASS] 1. seL4 kernel boots (dropped to user space)"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 1. seL4 kernel boot not detected"
    FAIL=$((FAIL + 1))
fi

# Check 2: WLTIOS banner
if grep -q "=== WLTIOS v0.1.0 ===" $LOG; then
    echo "[PASS] 2. WLTIOS v0.1.0 banner appears"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 2. WLTIOS banner missing"
    FAIL=$((FAIL + 1))
fi

# Check 3: wlt-init PID 1
if grep -q "wlt-init.*PID 1 starting" $LOG; then
    echo "[PASS] 3. wlt-init PID 1 starts"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 3. wlt-init PID 1 missing"
    FAIL=$((FAIL + 1))
fi

# Check 4: Network UP
if grep -q "Network UP" $LOG; then
    echo "[PASS] 4. Network UP"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 4. Network UP missing"
    FAIL=$((FAIL + 1))
fi

# Check 5: Shell prompt
if grep -q "/wlt#" $LOG; then
    echo "[PASS] 5. Shell prompt /wlt# appears"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 5. Shell prompt missing"
    FAIL=$((FAIL + 1))
fi

# Check 6: Tor bootstrap (if Tor binary is present)
if grep -q "Bootstrapped 100%" $LOG; then
    echo "[PASS] 6. Tor bootstrapped 100%"
    PASS=$((PASS + 1))
elif grep -q "Tor binary not found" $LOG; then
    echo "[SKIP] 6. Tor binary not bundled (expected if not yet added)"
else
    echo "[FAIL] 6. Tor bootstrap not reached"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
if [ $FAIL -eq 0 ]; then
    echo "OVERALL: PASS"
    exit 0
else
    echo "OVERALL: FAIL"
    exit 1
fi
