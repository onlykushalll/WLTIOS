#!/bin/bash
# A.4: Boot tor_pd_vm1 in QEMU (KVM), capture serial, prove Tor isolation.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/home/kushal/bin:/home/kushal/.local/bin

BUILD=/home/kushal/wltios-build/build-tor-pd-vm1
KERNEL=$BUILD/images/kernel-x86_64-pc99
LOADER=$BUILD/images/capdl-loader-image-x86_64-pc99
LOG=/home/kushal/torpd-vm1-isolation-serial.log

echo "[A.4] Booting tor_pd_vm1 (per-VM rootfs: vm0=no-tor, vm1=tor) in QEMU+KVM..."
echo "[A.4] Kernel: $KERNEL"
echo "[A.4] Loader: $LOADER"
echo "[A.4] Log: $LOG"

# Boot for 120 seconds - enough for both VMs to boot + init scripts + Tor start
timeout 120 qemu-system-x86_64 \
  -machine q35,accel=kvm,kernel-irqchip=split \
  -cpu host \
  -m 2G \
  -serial mon:stdio -display none -no-reboot \
  -kernel "$KERNEL" -initrd "$LOADER" > "$LOG" 2>&1

EXIT=$?
echo "[A.4] QEMU exited (code=$EXIT). Log size: $(stat -c%s "$LOG") bytes"

echo ""
echo "========== ISOLATION EVIDENCE =========="
echo "--- tor-pd messages (should be vm1 ONLY) ---"
grep "tor-pd" "$LOG" 2>/dev/null | head -10

echo ""
echo "--- VM0-PS section (should NOT have tor) ---"
sed -n '/VM0-PS-BEGIN/,/VM0-PS-END/p' "$LOG" 2>/dev/null | head -25

echo ""
echo "--- VM1-PS section (should HAVE tor) ---"
sed -n '/VM1-PS-BEGIN/,/VM1-PS-END/p' "$LOG" 2>/dev/null | head -25

echo ""
echo "--- vm-check lines ---"
grep "vm.-check" "$LOG" 2>/dev/null

echo ""
echo "--- boot summary ---"
echo "Welcome count: $(grep -c 'Welcome to Buildroot' "$LOG" 2>/dev/null)"
echo "login count: $(grep -c 'login:' "$LOG" 2>/dev/null)"
echo "VM_FATAL count: $(grep -c 'VM_FATAL' "$LOG" 2>/dev/null)"
echo "triple fault count: $(grep -ci 'triple fault' "$LOG" 2>/dev/null)"
echo "kernel panic count: $(grep -ci 'kernel panic' "$LOG" 2>/dev/null)"
echo "binfmt-464c (32-bit unsupported): $(grep -c 'binfmt-464c' "$LOG" 2>/dev/null)"
echo "ISA level errors: $(grep -c 'CPU ISA level' "$LOG" 2>/dev/null)"
