#!/bin/bash
# C.3-fix: Try -cpu max to enable all CPU features the host supports.
# The glibc CPU ISA level check fails because -cpu host in WSL2/KVM
# may not expose all features. -cpu max enables everything QEMU knows.
set -e
cd /home/kushal/wltios-build/build-tor-pd-vm1

LOG=/home/kushal/torpd-c3-cpu-max.log
echo "=== Boot with -cpu max ==="
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu max \
    -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== RESULTS ==="
echo "--- tor-pd messages ---"
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo "--- tor PIDs ---"
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo "--- tor log ---"
strings "$LOG" | grep -A20 "WLTIOS-TOR-LOG" | head -25
echo "--- CPU ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -5
