#!/bin/bash
# Download Alpine x86_64 Tor package only (no apt-get, faster).
set +e
mkdir -p /tmp/alpine-tor/extracted
cd /tmp/alpine-tor

echo "=== Download tor package ==="
timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.4.8.13-r0.apk" -o tor.apk
ls -la tor.apk 2>/dev/null
echo "--- Download deps ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libevent-2.1.12-r2.apk" -o libevent.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/openssl3-3.1.7-r0.apk" -o openssl3.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zlib-1.3.1-r0.apk" -o zlib.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libseccomp-2.5.5-r1.apk" -o libseccomp.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libcap-2.69-r1.apk" -o libcap.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/xz-libs-5.4.6-r0.apk" -o xz-libs.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zstd-libs-1.5.6-r0.apk" -o zstd-libs.apk
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/musl-1.2.5-r0.apk" -o musl.apk

echo ""
echo "=== Extract ==="
cd /tmp/alpine-tor/extracted
for apk in /tmp/alpine-tor/*.apk; do tar xzf "$apk" 2>/dev/null; done

echo ""
echo "=== Check Tor ==="
file usr/bin/tor 2>/dev/null
readelf -h usr/bin/tor 2>/dev/null | grep -E "Class|Machine"
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED
readelf -l usr/bin/tor 2>/dev/null | grep interpreter

echo ""
echo "=== Check deps ==="
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib"; fi
done

echo ""
echo "=== Vendor to packages/ ==="
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
mkdir -p "$REPO/packages/tor-musl-x86_64/lib"
cp usr/bin/tor "$REPO/packages/tor-musl-x86_64/tor"; chmod +x "$REPO/packages/tor-musl-x86_64/tor"
cp -a lib/lib*.so* lib/ld-musl-x86_64.so.1 "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
echo "Done:"
find "$REPO/packages/tor-musl-x86_64/" -type f | head -20
