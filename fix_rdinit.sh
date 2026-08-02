#!/bin/bash
# C.1-fix: Change cmdline to use rdinit=/init (run /init from initramfs directly)
# instead of root=/dev/mem init=/init (which fails because /dev/mem isn't a filesystem).
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== BEFORE ==="
grep "VM_GUEST_CMDLINE" "$CAMKES" | head -1

# Replace "root=/dev/mem init=/init" with "rdinit=/init"
# rdinit= tells the kernel to run /init from the initramfs (RAM disk) as PID 1,
# BEFORE attempting to mount a real root filesystem. This is the correct way
# to boot a guest VM that has no real disk (rootfs is entirely in initramfs).
sed -i 's|root=/dev/mem init=/init|rdinit=/init|' "$CAMKES"

echo ""
echo "=== AFTER ==="
grep "VM_GUEST_CMDLINE" "$CAMKES" | head -1

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5
