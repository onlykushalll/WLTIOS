#!/bin/bash
# Use APKINDEX to find correct versions + download all packages.
set +e

echo "=== 1. Download + parse APKINDEX ==="
mkdir -p /tmp/apk-index
cd /tmp/apk-index
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/APKINDEX.tar.gz" -o main.tar.gz
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/APKINDEX.tar.gz" -o comm.tar.gz
tar xzf main.tar.gz 2>/dev/null
mv APKINDEX APKINDEX_main 2>/dev/null
tar xzf comm.tar.gz 2>/dev/null
mv APKINDEX APKINDEX_comm 2>/dev/null

echo ""
echo "=== 2. Find exact versions ==="
for pkg in tor libevent openssl3 zlib libseccomp libcap xz-libs zstd-libs musl; do
    ver=$(grep -A2 "^P:$pkg$" APKINDEX_main APKINDEX_comm 2>/dev/null | grep "^V:" | head -1 | sed 's/^V://')
    repo=$(grep -B1 "^P:$pkg$" APKINDEX_main 2>/dev/null | head -1)
    if [ -n "$ver" ]; then
        echo "$pkg: $ver (main)"
    else
        ver=$(grep -A2 "^P:$pkg$" APKINDEX_comm 2>/dev/null | grep "^V:" | head -1 | sed 's/^V://')
        if [ -n "$ver" ]; then
            echo "$pkg: $ver (community)"
        else
            echo "$pkg: NOT FOUND"
        fi
    fi
done

echo ""
echo "=== 3. Download all with correct versions ==="
mkdir -p /tmp/alpine-correct
cd /tmp/alpine-correct

download_pkg() {
    local pkg=$1 repo=$2 ver=$3
    local url="https://dl-cdn.alpinelinux.org/alpine/v3.19/$repo/x86_64/${pkg}-${ver}.apk"
    echo -n "  $pkg-$ver from $repo: "
    timeout 15 curl -sL "$url" -o "${pkg}.apk"
    if file "${pkg}.apk" | grep -q "gzip"; then
        echo "OK ($(stat -c%s ${pkg}.apk) bytes)"
    else
        echo "FAIL (got HTML)"
        # Try alternative version format
        return 1
    fi
}

# Get versions and download
for pkg in musl zlib libseccomp xz-libs zstd-libs libcap libevent openssl3; do
    ver=$(grep -A2 "^P:$pkg$" /tmp/apk-index/APKINDEX_main 2>/dev/null | grep "^V:" | head -1 | sed 's/^V://')
    if [ -n "$ver" ]; then
        download_pkg "$pkg" "main" "$ver" || true
    else
        ver=$(grep -A2 "^P:$pkg$" /tmp/apk-index/APKINDEX_comm 2>/dev/null | grep "^V:" | head -1 | sed 's/^V://')
        if [ -n "$ver" ]; then
            download_pkg "$pkg" "community" "$ver" || true
        fi
    fi
done

# Tor is in community
tor_ver=$(grep -A2 "^P:tor$" /tmp/apk-index/APKINDEX_comm 2>/dev/null | grep "^V:" | head -1 | sed 's/^V://')
if [ -n "$tor_ver" ]; then
    download_pkg "tor" "community" "$tor_ver" || true
fi

echo ""
echo "=== 4. Extract all ==="
mkdir -p /tmp/alpine-correct/extracted
cd /tmp/alpine-correct/extracted
for apk in /tmp/alpine-correct/*.apk; do
    if file "$apk" | grep -q "gzip"; then
        tar xzf "$apk" 2>/dev/null || true
    fi
done

echo ""
echo "=== 5. Check Tor + deps ==="
file usr/bin/tor 2>/dev/null
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.musl-x86_64.so.1; do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib"; fi
done

echo ""
echo "=== 6. Build rootfs + boot ==="
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-alpine-correct

# Compile wlt-shell
gcc -static -o /tmp/wlt_s3 /tmp/wlt_s2.c 2>&1
echo "wlt-shell compiled"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_s3 bin/wlt-shell; chmod +x bin/wlt-shell

# Replace ALL libs with Alpine musl versions
rm -f lib/lib*.so* lib/ld-*
mkdir -p usr/bin etc/tor var/lib/tor lib
cp /tmp/alpine-correct/extracted/usr/bin/tor usr/bin/tor 2>/dev/null; chmod +x usr/bin/tor 2>/dev/null
cp -a /tmp/alpine-correct/extracted/lib/lib*.so* /tmp/alpine-correct/extracted/lib/ld-musl-x86_64.so.1 lib/ 2>/dev/null || true
cp -a /tmp/alpine-correct/extracted/usr/lib/lib*.so* lib/ 2>/dev/null || true
ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null || true

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
cp /tmp/wlt_s3 bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

# Rebuild
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

# Boot
LOG=/home/kushal/torpd-alpine-correct.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 7. Results ==="
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- lib errors ---"
strings "$LOG" | grep -i "shared lib\|cannot open\|error while loading\|relocating" | head -5
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
