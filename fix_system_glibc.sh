#!/bin/bash
# C.3-fix: Use WSL system's glibc (compiled for this actual CPU) instead of
# the wltios-rootfs64 glibc (which requires a newer ISA level).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-system-glibc

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

echo "=== Copy wlt-shell v3 ==="
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

echo "=== Copy Tor binary + torrc ==="
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

echo "=== Copy Tor-specific libs from wltios-rootfs64 (NOT libc/ld) ==="
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2; do
    for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        mkdir -p "$(dirname "$rel")"
        cp -P "$f" "$rel" 2>/dev/null || true
    done
done

echo "=== Copy WSL SYSTEM glibc (compiled for THIS CPU) ==="
# The WSL system glibc is compiled for the actual host CPU, so no ISA mismatch.
mkdir -p lib64
# ld-linux (dynamic linker)
cp /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 lib64/ld-linux-x86-64.so.2
# libc.so.6
cp /lib/x86_64-linux-gnu/libc.so.6 lib/libc.so.6
# libm.so.6 (math, also glibc)
cp /lib/x86_64-linux-gnu/libm.so.6 lib/libm.so.6
# Fix symlink
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

echo "=== Verify glibc version ==="
strings lib/libc.so.6 | grep "GLIBC_2\." | sort -V | tail -5
echo "ld-linux: $(file lib64/ld-linux-x86-64.so.2)"

echo "=== Check ALL Tor deps with system glibc ==="
LD_LIBRARY_PATH="$WORK/lib:$WORK/lib64" "$WORK/lib64/ld-linux-x86-64.so.2" --list "$WORK/usr/bin/tor" 2>&1 | head -20

echo ""
echo "=== Build vm0 ==="
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

echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c3-system-glibc.log
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
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo "--- tor log ---"
strings "$LOG" | grep -A20 "WLTIOS-TOR-LOG" | head -25
echo "--- CPU ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
