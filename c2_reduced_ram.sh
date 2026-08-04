#!/bin/bash
# C.2 FIX v2: The seL4 root server (capdl-loader) needs more untyped memory.
# The error is at the rootserver level, not the VM level.
# Fix: increase QEMU RAM + reduce VM guest_ram to leave more for seL4.
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== Current config ==="
grep "guest_ram\|heap_size\|untyped.*pool" "$CAMKES" | head -10

# Revert heap_size to original (0x10000), keep untyped pools at 40
sed -i 's/vm0.heap_size = 0x200000;/vm0.heap_size = 0x10000;/' "$CAMKES"
sed -i 's/vm1.heap_size = 0x200000;/vm1.heap_size = 0x10000;/' "$CAMKES"

# Reduce guest_ram from 128MB to 64MB to leave more for seL4
sed -i 's/vm0.guest_ram_mb = 128;/vm0.guest_ram_mb = 64;/' "$CAMKES"
sed -i 's/vm1.guest_ram_mb = 128;/vm1.guest_ram_mb = 64;/' "$CAMKES"

echo ""
echo "=== After fix ==="
grep "guest_ram\|heap_size\|untyped.*pool" "$CAMKES" | head -10

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
cmake . 2>&1 | tail -3
ninja 2>&1 | tail -3

echo ""
echo "=== Boot with 4G RAM (more for seL4) ==="
LOG=/home/kushal/torpd-c2-reduced-ram.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Results ==="
echo "--- Insufficient memory ---"
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -3
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
