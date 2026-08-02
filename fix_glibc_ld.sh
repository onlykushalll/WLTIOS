#!/bin/bash
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-glibc-ld

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

echo "=== Base rootfs lib64/ld-linux ==="
file lib64/ld-linux-x86-64.so.2 2>/dev/null
ls -la lib64/ld-linux-x86-64.so.2 2>/dev/null
readlink -f lib64/ld-linux-x86-64.so.2 2>/dev/null

echo ""
echo "=== Base rootfs lib/ld-linux ==="
file lib/ld-linux-x86-64.so.2 2>/dev/null
ls -la lib/ld-linux-x86-64.so.2 2>/dev/null

echo ""
echo "=== wltios-rootfs64 lib64/ld-linux ==="
file "$TOR_SRC/lib64/ld-linux-x86-64.so.2" 2>/dev/null
ls -la "$TOR_SRC/lib64/ld-linux-x86-64.so.2" 2>/dev/null

echo ""
echo "=== Copy wlt-shell v3 ==="
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

echo "=== Copy ONLY Tor-specific libs (NOT ld-linux) ==="
# Copy Tor binary + torrc
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

# Copy ONLY the specific libs Tor needs, but DON'T overwrite ld-linux
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6; do
    for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        dest_dir="$(dirname "$rel")"
        mkdir -p "$dest_dir"
        # Don't overwrite if already exists in base rootfs
        if [ ! -f "$rel" ]; then
            cp -P "$f" "$rel" 2>/dev/null || true
        fi
    done
done

echo ""
echo "=== Also copy glibc ld-linux from WSL system ==="
# The base rootfs's ld-linux might be musl. Copy the system glibc ld.
cp /lib64/ld-linux-x86-64.so.2 lib64/ld-linux-x86-64.so.2 2>/dev/null || true
cp /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2 2>/dev/null || true
file lib64/ld-linux-x86-64.so.2

echo ""
echo "=== Verify Tor can find all deps ==="
# Use the system ld-linux to check Tor's deps
LD_LIBRARY_PATH="$WORK/lib:$WORK/lib64" /lib64/ld-linux-x86-64.so.2 --list "$WORK/usr/bin/tor" 2>&1 | head -15

echo ""
echo "=== Build rootfs ==="
find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

# vm0 (just wlt-shell, no Tor)
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

echo ""
echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c1-glibc-ld.log
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
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
