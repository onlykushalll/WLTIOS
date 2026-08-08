#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd /home/kushal/wltios-build/build-tor-pd-vm1
LOG=/home/kushal/torpd-tor-first.log
echo "Booting (120s)..."
timeout 120 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -15
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
