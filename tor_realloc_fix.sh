#!/bin/bash
# Add pci=realloc=off to prevent kernel from reassigning BARs + boot.
set +e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== Add pci=realloc=off ==="
if ! grep -q "realloc=off" "$CAMKES"; then
    sed -i 's/pci=nomsi/pci=nomsi realloc=off/' "$CAMKES"
    echo "Added pci=realloc=off"
else
    echo "Already present"
fi
grep VM_GUEST_CMDLINE "$CAMKES" | head -1

echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo "=== Boot (120s) ==="
LOG=/home/kushal/torpd-torfix-realloc.log
timeout 120 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== Results ==="
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -15
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- ISA ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
echo "--- PCI errors ---"
strings "$LOG" | grep "error updating\|can't claim" | head -5
echo "--- Last 15 ---"
strings "$LOG" | tail -15
