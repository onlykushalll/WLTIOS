#!/bin/bash
# Download missing deps (openssl3, libcap) + build rootfs + boot test.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
MUSL_DIR=/tmp/musl-tor/extracted

echo "=== 1. Find + download missing deps ==="
cd /tmp
# openssl3 — search the main APKINDEX
openssl_ver=$(grep -A2 "^P:openssl3$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
echo "openssl3 version: $openssl_ver"
if [ -n "$openssl_ver" ]; then
    timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/openssl3-${openssl_ver}.apk" -o /tmp/musl-tor/openssl3.apk
    ls -la /tmp/musl-tor/openssl3.apk
fi

# libcap — might be in a different package name
libcap_ver=$(grep -A2 "^P:libcap$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
echo "libcap version: $libcap_ver"
if [ -n "$libcap_ver" ]; then
    timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/libcap-${libcap_ver}.apk" -o /tmp/musl-tor/libcap.apk
    ls -la /tmp/musl-tor/libcap.apk
fi

echo ""
echo "=== 2. Extract missing deps ==="
cd "$MUSL_DIR"
for apk in /tmp/musl-tor/openssl3.apk /tmp/musl-tor/libcap.apk; do
    if [ -f "$apk" ]; then
        echo "Extracting $(basename $apk)..."
        tar xzf "$apk" 2>/dev/null || true
    fi
done

echo ""
echo "=== 3. Re-check all deps ==="
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

echo ""
echo "=== 4. Update vendored packages/ ==="
rm -rf "$REPO/packages/tor-musl-x86_64/lib"
mkdir -p "$REPO/packages/tor-musl-x86_64/lib"
cp usr/bin/tor "$REPO/packages/tor-musl-x86_64/tor"; chmod +x "$REPO/packages/tor-musl-x86_64/tor"
# Copy ALL libs from lib/ and usr/lib/
cp -a lib/lib*.so* lib/ld-musl-x86_64.so.1 "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
cp -a usr/lib/lib*.so* "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
echo "Vendored:"
find "$REPO/packages/tor-musl-x86_64/" -type f | head -20
echo "Size: $(du -sh "$REPO/packages/tor-musl-x86_64/" 2>/dev/null | cut -f1)"

echo ""
echo "=== 5. Build vm1 rootfs with musl-Tor ==="
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-musl-tor

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Replace /bin/wlt-shell with v12
if [ -f /tmp/wlt_shell_v12 ]; then
    cp /tmp/wlt_shell_v12 bin/wlt-shell; chmod +x bin/wlt-shell
else
    echo "WARNING: wlt_shell_v12 not found, using existing wlt-shell"
fi

# Copy musl-Tor + ALL deps
mkdir -p usr/bin etc/tor var/lib/tor lib
cp "$MUSL_DIR/usr/bin/tor" usr/bin/tor; chmod +x usr/bin/tor
# Copy ALL shared libs from musl-tor extraction
cp -a "$MUSL_DIR/lib/"* lib/ 2>/dev/null || true
cp -a "$MUSL_DIR/usr/lib/"* lib/ 2>/dev/null || true
# Fix symlinks (musl ld)
if [ -f lib/ld-musl-x86_64.so.1 ]; then
    ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null || true
fi

# Create torrc
cat > etc/tor/torrc << 'TORRC'
DataDirectory /var/lib/tor
Log notice file /tmp/tor.log
SocksPort 9050
ExitPolicy reject *:*
User root
TORRC

# Build rootfs
find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 6. Rebuild seL4 image ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== 7. Boot test (90s) ==="
LOG=/home/kushal/torpd-musl-tor.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 8. Results ==="
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
