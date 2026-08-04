#!/bin/bash
# B.1: Run pci=probeall test. The script was prepared last session but bridge went down.
# Also add deeper VMM tracing: check if vmm_pci_add_entry is called during boot.
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== Current cmdline ==="
grep VM_GUEST_CMDLINE "$CAMKES" | head -1

# Add pci=probeall if not already there
if ! grep -q "pci=probeall" "$CAMKES"; then
    sed -i 's/pci=nomsi/pci=probeall/' "$CAMKES"
    echo "Added pci=probeall"
else
    echo "pci=probeall already present"
fi

echo ""
echo "=== After fix ==="
grep VM_GUEST_CMDLINE "$CAMKES" | head -1

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo ""
echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-probeall-final.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Results ==="
echo "--- PCI scan messages ---"
strings "$LOG" | grep -i "pci 0000\|pci_bus\|pci_dev\|probe\|scan" | head -15
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices found ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo "--- VMM init messages ---"
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|connection.*init\|vmm_pci\|Finalising\|mutex" | head -10
echo "--- Insufficient memory ---"
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -3
