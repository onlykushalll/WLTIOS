#!/bin/bash
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-all-libs

echo "=== Extract base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

echo "=== Copy wlt-shell v3 binary ==="
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

echo "=== Copy ALL libraries from wltios-rootfs64 (including musl) ==="
# Copy ALL of lib/ and lib64/ from wltios-rootfs64, merging with existing
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
cp -a "$TOR_SRC/lib64/"* lib64/ 2>/dev/null || true

echo "=== Verify musl libs present ==="
ls -la lib/libc.musl-x86_64.so.1 lib/ld-musl-x86_64.so.1 2>/dev/null
ls -la lib64/ld-linux-x86-64.so.2 2>/dev/null
ls -la lib/ld-linux-x86-64.so.2 2>/dev/null

echo "=== Copy Tor binary + torrc ==="
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

echo "=== Check Tor deps ==="
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | head -15

echo "=== Check all deps exist ==="
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find lib lib64 -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then
        echo "MISSING: $lib"
    else
        echo "OK: $lib -> $found"
    fi
done

# vm0
echo "=== Build vm0 ==="
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1 (already has Tor + libs from above)
echo "=== Build vm1 ==="
find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c1-all-libs.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== RESULTS ==="
echo "--- tor-pd messages ---"
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo "--- tor in PID lines ---"
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor processes" | head -10
echo "--- tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- all unique PIDs ---"
strings "$LOG" | grep "^PID " | sort -u | head -40
