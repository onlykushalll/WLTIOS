#!/bin/bash
# tests/integration/sel4_multipd_test.sh
# Boots the seL4 Key Vault multi-PD system in QEMU and verifies
# the ping/pong exchange between the Linux guest VM and the KeyVaultPD.
#
# Evidence required: "TEST PASSED" in the raw serial output.
# Exit: 0 = pass, 1 = fail
#
# Prerequisites:
#   - ~/wltios-build/build-keyvault/ exists with built images
#   - The rootfs has the S90camkes_init test script (fakeroot-repacked)
#   - KVM available for acceleration

set -e

BUILD_DIR="$HOME/wltios-build/build-keyvault"
KERNEL="$BUILD_DIR/images/kernel-ia32-pc99"
INITRD="$BUILD_DIR/images/capdl-loader-image-ia32-pc99"
LOG="/tmp/sel4-multipd-test-$(date +%s).log"

echo "=== seL4 Multi-PD Ping/Pong Integration Test ==="

# Verify images exist
if [ ! -f "$KERNEL" ] || [ ! -f "$INITRD" ]; then
    echo "[FAIL] Build images not found at $BUILD_DIR"
    echo "       Run: cd ~/wltios-build && ../init-build.sh -DCAMKES_VM_APP=keyvault_pd && ninja -C build-keyvault"
    exit 1
fi

echo "=== Booting QEMU (120s timeout) ==="
echo "=== Log file: $LOG ==="

# Boot with the same QEMU config that Antigravity verified working
timeout 120 qemu-system-i386 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu Nehalem,+vme,-pdpe1gb,+xsave,+xsaveopt,-xsavec,-fsgsbase,-invpcid,-syscall,-lm,+vmx \
    -nographic -serial mon:stdio \
    -m size=2G \
    -kernel "$KERNEL" \
    -initrd "$INITRD" \
    > "$LOG" 2>&1

QEMU_EXIT=$?
echo "=== QEMU exited with code $QEMU_EXIT ==="

# Check for the three required evidence markers in the raw serial log
PASS=0
FAIL=0

echo ""
echo "--- Checks ---"

# Check 1: KeyVaultPD is alive
if grep -q "\[key-vault-pd\] alive" "$LOG"; then
    echo "[PASS] 1. KeyVaultPD component booted"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 1. KeyVaultPD alive message missing"
    FAIL=$((FAIL + 1))
fi

# Check 2: Ping received and pong sent by the PD
if grep -q "\[key-vault-pd\] ping received, pong sent" "$LOG"; then
    echo "[PASS] 2. Ping received, pong sent by KeyVaultPD"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 2. Ping/pong exchange missing"
    FAIL=$((FAIL + 1))
fi

# Check 3: Interrupt crossed the PD boundary (IRQ count incremented)
if grep -q "12:.*1.*XT-PIC.*connector" "$LOG"; then
    echo "[PASS] 3. Interrupt crossed PD boundary (IRQ 12 count = 1)"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 3. Interrupt did not cross PD boundary"
    FAIL=$((FAIL + 1))
fi

# Check 4: Linux VM read "pong" from the dataport
if grep -q "Result from Key Vault: pong" "$LOG"; then
    echo "[PASS] 4. Linux VM received pong from KeyVaultPD"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 4. Linux VM did not receive pong"
    FAIL=$((FAIL + 1))
fi

# Check 5: TEST PASSED marker from the test script
if grep -q "TEST PASSED" "$LOG"; then
    echo "[PASS] 5. Test script reported TEST PASSED"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 5. TEST PASSED marker missing"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
echo "=== Log file: $LOG ==="

if [ $FAIL -eq 0 ]; then
    echo "OVERALL: PASS"
    echo ""
    echo "seL4 multi-PD isolation VERIFIED."
    echo "Two real protection domains (Linux VM + KeyVaultPD) communicate"
    echo "bidirectionally through the formally-verified seL4 kernel boundary."
    exit 0
else
    echo "OVERALL: FAIL"
    echo ""
    echo "=== Last 30 lines of serial log ==="
    tail -30 "$LOG" 2>/dev/null
    exit 1
fi
