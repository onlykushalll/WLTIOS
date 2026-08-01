#!/bin/bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/home/kushal/bin:/home/kushal/.local/bin
BUILD=/home/kushal/wltios-build/build-tor-pd-vm1
KERNEL=$BUILD/images/kernel-x86_64-pc99
LOADER=$BUILD/images/capdl-loader-image-x86_64-pc99
LOG=/home/kushal/torpd-vm1-isolation-serial.log
echo "Booting tor_pd_vm1 in QEMU+KVM (90s timeout)..."
timeout 90 qemu-system-x86_64 \
  -machine q35,accel=kvm,kernel-irqchip=split \
  -cpu host \
  -m 2G \
  -serial mon:stdio -display none -no-reboot \
  -kernel "$KERNEL" -initrd "$LOADER" > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(stat -c%s "$LOG")"
