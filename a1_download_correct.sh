#!/bin/bash
# Download the correct Alpine Tor package (v0.4.9.11-r0) + all deps + build rootfs.
set +e
mkdir -p /tmp/musl-tor/extracted
cd /tmp/musl-tor

echo "=== 1. Download tor-0.4.9.11-r0.apk ==="
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/tor-0.4.9.11-r0.apk" -o tor.apk
ls -la tor.apk
file tor.apk

echo ""
echo "=== 2. Download deps ==="
# Get the APKINDEX to find exact dep versions
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/APKINDEX.tar.gz" -o /tmp/APKINDEX-main.tar.gz
cd /tmp && tar xzf APKINDEX-main.tar.gz 2>/dev/null

# Find dep versions from the index
for pkg in libevent openssl3 zlib libseccomp libcap xz-libs zstd-libs musl; do
    ver=$(grep -A2 "^P:$pkg$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
    echo "$pkg version: $ver"
    if [ -n "$ver" ]; then
        timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/${pkg}-${ver}.apk" -o "/tmp/musl-tor/${pkg}.apk"
        ls -la "/tmp/musl-tor/${pkg}.apk" 2>/dev/null
    fi
done

# Also get libevent from community (might be there)
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/APKINDEX.tar.gz" -o /tmp/APKINDEX-comm.tar.gz
cd /tmp && tar xzf APKINDEX-comm.tar.gz 2>/dev/null
libevent_ver=$(grep -A2 "^P:libevent$" /tmp/APKINDEX | grep "^V:" | head -1 | sed 's/^V://')
echo "libevent version: $libevent_ver"
if [ -n "$libevent_ver" ]; then
    timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/libevent-${libevent_ver}.apk" -o "/tmp/musl-tor/libevent.apk"
    ls -la "/tmp/musl-tor/libevent.apk" 2>/dev/null
fi

echo ""
echo "=== 3. Extract all ==="
cd /tmp/musl-tor/extracted
for apk in /tmp/musl-tor/*.apk; do
    echo "Extracting $(basename $apk)..."
    tar xzf "$apk" 2>/dev/null || true
done

echo ""
echo "=== 4. Check Tor binary ==="
file usr/bin/tor 2>/dev/null
readelf -h usr/bin/tor 2>/dev/null | grep -E "Class|Machine"
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED
readelf -l usr/bin/tor 2>/dev/null | grep interpreter

echo ""
echo "=== 5. Check all deps ==="
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

echo ""
echo "=== 6. Vendor to packages/ ==="
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
rm -rf "$REPO/packages/tor-musl-x86_64"
mkdir -p "$REPO/packages/tor-musl-x86_64/lib"
cp usr/bin/tor "$REPO/packages/tor-musl-x86_64/tor"; chmod +x "$REPO/packages/tor-musl-x86_64/tor"
cp -a lib/lib*.so* "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
cp lib/ld-musl-x86_64.so.1 "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
echo "Vendored files:"
find "$REPO/packages/tor-musl-x86_64/" -type f | head -20
echo "Total size: $(du -sh "$REPO/packages/tor-musl-x86_64/" 2>/dev/null | cut -f1)"
