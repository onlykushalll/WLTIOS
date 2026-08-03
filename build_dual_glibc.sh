#!/bin/bash
# C.3-fix v2: Copy BOTH system glibc (for Tor) AND musl libs (for libsystemd).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-dual-glibc

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

# Copy ALL libs from wltios-rootfs64/lib/ (includes musl + all Tor deps)
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
# Copy lib64/ld-linux from wltios-rootfs64
cp "$TOR_SRC/lib64/ld-linux-x86-64.so.2" lib64/ld-linux-x86-64.so.2 2>/dev/null || true

# OVERWRITE glibc with WSL system glibc (compiled for THIS CPU)
cp /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 lib64/ld-linux-x86-64.so.2
cp /lib/x86_64-linux-gnu/libc.so.6 lib/libc.so.6
cp /lib/x86_64-linux-gnu/libm.so.6 lib/libm.so.6
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

echo "=== Verify musl libs present ==="
ls -la lib/ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null
echo "=== Verify glibc present ==="
ls -la lib/libc.so.6 lib/libm.so.6 lib64/ld-linux-x86-64.so.2 2>/dev/null

echo "=== Tor deps check ==="
LD_LIBRARY_PATH="$WORK/lib:$WORK/lib64" "$WORK/lib64/ld-linux-x86-64.so.2" --list "$WORK/usr/bin/tor" 2>&1 | head -20

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild seL4 image ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3
echo "BUILD DONE"
