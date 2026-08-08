#!/bin/bash
# Download ALL Alpine musl deps + rebuild rootfs + boot.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-musl-alldeps

echo "=== 1. Download ALL Alpine musl deps ==="
mkdir -p /tmp/alpine-deps
cd /tmp/alpine-deps

# Get the APKINDEX for exact versions
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/APKINDEX.tar.gz" -o APKINDEX-main.tar.gz
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/APKINDEX.tar.gz" -o APKINDEX-comm.tar.gz
cd /tmp && tar xzf /tmp/alpine-deps/APKINDEX-main.tar.gz 2>/dev/null
tar xzf /tmp/alpine-deps/APKINDEX-comm.tar.gz -C /tmp 2>/dev/null

# Download each dep with correct version from APKINDEX
for pkg in musl zlib libseccomp xz-libs zstd-libs libcap libevent openssl3; do
    ver=$(grep -A2 "^P:$pkg$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
    repo="main"
    if [ -z "$ver" ]; then
        ver=$(grep -A2 "^P:$pkg$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
        repo="community"
    fi
    if [ -n "$ver" ]; then
        echo "Downloading $pkg-$ver from $repo..."
        timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/$repo/x86_64/${pkg}-${ver}.apk" -o "/tmp/alpine-deps/${pkg}.apk"
        ls -la "/tmp/alpine-deps/${pkg}.apk" 2>/dev/null
    else
        echo "NOT FOUND: $pkg"
    fi
done

echo ""
echo "=== 2. Extract all ==="
mkdir -p /tmp/alpine-deps/extracted
cd /tmp/alpine-deps/extracted
for apk in /tmp/alpine-deps/*.apk; do
    echo "Extracting $(basename $apk)..."
    tar xzf "$apk" 2>/dev/null || true
done

echo ""
echo "=== 3. Verify all deps ==="
TOR=/tmp/alpine-deps/extracted/usr/bin/tor
if [ ! -f "$TOR" ]; then
    # Tor is in the community repo
    timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/tor-0.4.9.11-r0.apk" -o /tmp/alpine-deps/tor.apk
    tar xzf /tmp/alpine-deps/tor.apk -C /tmp/alpine-deps/extracted 2>/dev/null
    TOR=/tmp/alpine-deps/extracted/usr/bin/tor
fi
file "$TOR" 2>/dev/null

for lib in $(readelf -d "$TOR" 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find /tmp/alpine-deps/extracted -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

echo ""
echo "=== 4. Vendor ALL musl deps to packages/ ==="
rm -rf "$REPO/packages/tor-musl-x86_64/lib"
mkdir -p "$REPO/packages/tor-musl-x86_64/lib"
cp /tmp/alpine-deps/extracted/usr/bin/tor "$REPO/packages/tor-musl-x86_64/tor"; chmod +x "$REPO/packages/tor-musl-x86_64/tor"
# Copy ALL libs (from lib/ and usr/lib/)
cp -a /tmp/alpine-deps/extracted/lib/lib*.so* /tmp/alpine-deps/extracted/lib/ld-musl-x86_64.so.1 "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
cp -a /tmp/alpine-deps/extracted/usr/lib/lib*.so* "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
echo "Vendored:"
find "$REPO/packages/tor-musl-x86_64/" -type f | head -20
echo "Size: $(du -sh "$REPO/packages/tor-musl-x86_64/" 2>/dev/null | cut -f1)"

echo ""
echo "=== 5. Build rootfs with ALL musl deps ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# 64-bit static wlt-shell
if [ -f /tmp/wlt_simple ]; then
    cp /tmp/wlt_simple bin/wlt-shell; chmod +x bin/wlt-shell
else
    echo "ERROR: wlt_simple not found"
    exit 1
fi

# musl-Tor + ALL Alpine musl deps (NOT glibc deps)
rm -f lib/lib*.so* lib/ld-*
mkdir -p usr/bin etc/tor var/lib/tor lib
cp "$REPO/packages/tor-musl-x86_64/tor" usr/bin/tor; chmod +x usr/bin/tor
cp -a "$REPO/packages/tor-musl-x86_64/lib/"* lib/ 2>/dev/null || true
# Fix musl symlinks
if [ -f lib/ld-musl-x86_64.so.1 ]; then
    ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null || true
fi

cat > etc/tor/torrc << 'TORRC'
DataDirectory /var/lib/tor
Log notice file /tmp/tor.log
SocksPort 9050
ExitPolicy reject *:*
User root
TORRC

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_simple bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== 7. Boot (90s) ==="
LOG=/home/kushal/torpd-musl-alldeps.log
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
echo "--- shared lib errors ---"
strings "$LOG" | grep -i "shared lib\|cannot open\|error while loading\|relocating" | head -5
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
