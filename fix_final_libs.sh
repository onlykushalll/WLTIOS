#!/bin/bash
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-final-libs

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

echo "=== Copy wlt-shell v3 ==="
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

echo "=== Copy Tor + ALL deps from wltios-rootfs64 ==="
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

# Copy ALL libs from wltios-rootfs64/lib/ (including musl)
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true

# Copy lib64/ld-linux from wltios-rootfs64 (the real glibc ld, 254864 bytes)
cp "$TOR_SRC/lib64/ld-linux-x86-64.so.2" lib64/ld-linux-x86-64.so.2

# Fix the broken symlink: lib/ld-linux-x86-64.so.2 -> /lib64/ld-linux-x86-64.so.2
# (absolute path won't work in guest; make it relative)
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

echo "=== Verify all libs present ==="
echo "lib64/ld-linux-x86-64.so.2: $(file lib64/ld-linux-x86-64.so.2 | grep -o 'ELF.*executable\|symbolic link')"
echo "lib/libc.so.6: $(ls -la lib/libc.so.6 2>/dev/null | head -1)"
echo "lib/libc.musl-x86_64.so.1: $(ls -la lib/libc.musl-x86_64.so.1 2>/dev/null | head -1)"
echo "lib/ld-musl-x86_64.so.1: $(ls -la lib/ld-musl-x86_64.so.1 2>/dev/null | head -1)"
echo "lib/ld-linux-x86-64.so.2: $(ls -la lib/ld-linux-x86-64.so.2 2>/dev/null | head -1)"

echo ""
echo "=== Check ALL Tor deps (including transitive) ==="
# Use the glibc ld to list Tor's dependencies
LD_LIBRARY_PATH="$WORK/lib:$WORK/lib64" "$WORK/lib64/ld-linux-x86-64.so.2" --list "$WORK/usr/bin/tor" 2>&1 | head -20

echo ""
echo "=== Build vm0 ==="
# vm0: just wlt-shell, no Tor
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

echo "=== Build vm1 ==="
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c1-final-libs.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== RESULTS ==="
echo "--- tor-pd ---"
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo "--- tor PIDs ---"
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor processes" | head -10
echo "--- tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
