#!/bin/bash
# C.2 FIX: Increase untyped memory pools so VMM can allocate cross_vm connection.
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== BEFORE: untyped pools ==="
grep "untyped.*pool\|heap_size" "$CAMKES" | head -10

# Increase untyped pools to give VMM enough memory for cross_vm connections
# The error was: "Insufficient memory (1 * 2097152 bytes needed, 0 bytes available)"
# = needs 2MB more. Increase simple_untyped23_pool from 21 to 40.
sed -i 's/vm0.simple_untyped23_pool = 21;/vm0.simple_untyped23_pool = 40;/' "$CAMKES"
sed -i 's/vm1.simple_untyped23_pool = 21;/vm1.simple_untyped23_pool = 40;/' "$CAMKES"
# Also increase heap_size (was 0x10000 = 64KB, increase to 0x100000 = 1MB)
sed -i 's/vm0.heap_size = 0x10000;/vm0.heap_size = 0x200000;/' "$CAMKES"
sed -i 's/vm1.heap_size = 0x10000;/vm1.heap_size = 0x200000;/' "$CAMKES"

echo ""
echo "=== AFTER: untyped pools ==="
grep "untyped.*pool\|heap_size" "$CAMKES" | head -10

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
cmake . 2>&1 | tail -3
ninja 2>&1 | tail -5

echo ""
echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-more-mem.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Results ==="
echo "--- Insufficient memory errors ---"
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -5
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x1af4\|vendor=0x" | head -10
echo "--- cross_vm init ---"
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|connection.*init" | head -5
