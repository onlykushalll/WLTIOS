#!/bin/bash
# B.2 FIX v4: Increase guest RAM back to 128MB (64MB was too small for vm1's rootfs).
set +e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== Increase guest RAM to 128MB ==="
sed -i 's/vm0.guest_ram_mb = 64;/vm0.guest_ram_mb = 128;/' "$CAMKES"
sed -i 's/vm1.guest_ram_mb = 64;/vm1.guest_ram_mb = 128;/' "$CAMKES"
grep "guest_ram_mb" "$CAMKES" | head -2

echo ""
echo "=== Revert pci=probeall back to pci=nomsi ==="
sed -i 's/pci=probeall/pci=nomsi/' "$CAMKES"
grep VM_GUEST_CMDLINE "$CAMKES" | head -1

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo ""
echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-128mb.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Results ==="
echo "--- Assertion errors ---"
strings "$LOG" | grep "Assertion\|assert" | head -3
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo "--- VMs booted ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "RAMDISK: $(strings "$LOG" | grep -c 'RAMDISK')"
strings "$LOG" | grep "RAMDISK" | head -5
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5
echo "--- cross_vm init ---"
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|Finalising\|connection" | head -5
echo "--- Insufficient memory ---"
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -3
