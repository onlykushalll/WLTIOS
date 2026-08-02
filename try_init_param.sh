#!/bin/bash
# Try init=/init instead of rdinit=/init. Also try adding root=/dev/ram0.
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== BEFORE ==="
grep "VM_GUEST_CMDLINE" "$CAMKES" | head -1

# Change rdinit=/init to init=/init (different kernel code path)
sed -i 's|rdinit=/init|init=/init|' "$CAMKES"

echo ""
echo "=== AFTER ==="
grep "VM_GUEST_CMDLINE" "$CAMKES" | head -1

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5

echo ""
echo "=== Boot test ==="
LOG=/home/kushal/torpd-c1-init-test.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Check for init markers ==="
strings "$LOG" | grep "WLTIOS-INIT-RAN\|WLTIOS-PS\|wlt-check\|Run /init\|Starting init\|Failed to execute" | head -15

echo ""
echo "=== Last 15 lines ==="
strings "$LOG" | tail -15
