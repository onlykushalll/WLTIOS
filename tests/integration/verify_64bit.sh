#!/bin/bash
# verify_64bit.sh — Boot the 64-bit QEMU guest and verify the 5 serial lines
# Reports PASS/FAIL for each check.

LOG=/home/kushal/wltios-build/wltios-64bit-verify.log
PASS=0
FAIL=0

echo "=== WLTIOS 64-bit Full Boot Verification ==="

# Boot QEMU with piped commands
{
  sleep 120
  printf "netcheck\n"; sleep 5
  printf "tstatus\n"; sleep 4
  printf "ls /run/\n"; sleep 3
  printf "ls /dev/dri/\n"; sleep 2
  printf "wayland_test\n"; sleep 5
  printf "exit\n"
} | timeout 300 qemu-system-x86_64 \
  -kernel /home/kushal/wltios-build/kernel-amd64 \
  -initrd /home/kushal/wltios-build/wltios-initramfs64.cpio.gz \
  -append "console=ttyS0 root=/dev/ram rdinit=/init" \
  -m 512 -nographic -no-reboot \
  -netdev user,id=n0 -device e1000,netdev=n0 \
  -device virtio-gpu-pci \
  > $LOG 2>&1

echo ""
echo "--- Checks ---"

# Check 1: WLTIOS banner
if grep -q "=== WLTIOS v0.1.0 ===" $LOG; then
    echo "[PASS] 1. WLTIOS v0.1.0 banner appears"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 1. WLTIOS v0.1.0 banner missing"
    FAIL=$((FAIL + 1))
fi

# Check 2: Network UP
if grep -q "Network UP (10.0.2.15" $LOG; then
    echo "[PASS] 2. Network UP (10.0.2.15/24)"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 2. Network UP missing"
    FAIL=$((FAIL + 1))
fi

# Check 3: Tor bootstrap 100%
if grep -q "Bootstrapped 100%" $LOG; then
    echo "[PASS] 3. Tor bootstrapped 100%"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 3. Tor 100% bootstrap missing (may need more time)"
    FAIL=$((FAIL + 1))
fi

# Check 4: Compositor wayland-0
if grep -q "Wayland display running on wayland-0" $LOG; then
    echo "[PASS] 4. Compositor running, wayland-0 created"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 4. Compositor wayland-0 missing"
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

# Check 6: ls /run/ shows wayland-0
if grep -A2 "ls /run/" $LOG | grep -q "wayland-0"; then
    echo "[PASS] 6. ls /run/ shows wayland-0"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 6. wayland-0 not in /run/"
    FAIL=$((FAIL + 1))
fi

# Check 7: DRM card0 and renderD128 devices
if grep -q "card0" $LOG && grep -q "renderD128" $LOG; then
    echo "[PASS] 7. DRM card0 and renderD128 devices created"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 7. DRM devices missing"
    FAIL=$((FAIL + 1))
fi

# Check 8: Compositor DRM backend
if grep -q "DRM backend created" $LOG; then
    echo "[PASS] 8. Compositor initialized with DRM backend"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 8. DRM backend not initialized"
    FAIL=$((FAIL + 1))
fi

# Check 9: Wayland test client
if grep -q "Wayland surface committed" $LOG; then
    echo "[PASS] 9. Wayland test client ran and committed surface"
    PASS=$((PASS + 1))
else
    echo "[FAIL] 9. Wayland test client failed"
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
