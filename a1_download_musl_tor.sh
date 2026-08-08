#!/bin/bash
# A.1-A.3: Download Alpine x86_64 Tor package (musl-compiled 64-bit) + extract.
set +e

echo "=== 1. Install musl-tools (provides musl-gcc) ==="
sudo apt-get install -y musl-tools 2>&1 | tail -3
which musl-gcc

echo ""
echo "=== 2. Download Alpine x86_64 Tor package ==="
# Alpine packages are .apk files (gzipped tarballs)
mkdir -p /tmp/alpine-tor
cd /tmp/alpine-tor

# Download tor package from Alpine v3.19
echo "--- Downloading tor ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.4.8.13-r0.apk" -o tor.apk 2>&1
ls -la tor.apk 2>/dev/null
file tor.apk 2>/dev/null

# Download tor's dependencies
echo "--- Downloading libevent ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libevent-2.1.12-r2.apk" -o libevent.apk 2>&1
ls -la libevent.apk 2>/dev/null

echo "--- Downloading openssl (libssl+libcrypto) ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/openssl3-3.1.7-r0.apk" -o openssl3.apk 2>&1
ls -la openssl3.apk 2>/dev/null

echo "--- Downloading zlib ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zlib-1.3.1-r0.apk" -o zlib.apk 2>&1
ls -la zlib.apk 2>/dev/null

echo "--- Downloading libseccomp ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libseccomp-2.5.5-r1.apk" -o libseccomp.apk 2>&1
ls -la libseccomp.apk 2>/dev/null

echo "--- Downloading libcap ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libcap-2.69-r1.apk" -o libcap.apk 2>&1
ls -la libcap.apk 2>/dev/null

echo "--- Downloading xz-libs (liblzma) ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/xz-libs-5.4.6-r0.apk" -o xz-libs.apk 2>&1
ls -la xz-libs.apk 2>/dev/null

echo "--- Downloading zstd-libs ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zstd-libs-1.5.6-r0.apk" -o zstd-libs.apk 2>&1
ls -la zstd-libs.apk 2>/dev/null

echo "--- Downloading musl ---"
timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/musl-1.2.5-r0.apk" -o musl.apk 2>&1
ls -la musl.apk 2>/dev/null

echo ""
echo "=== 3. Extract all packages ==="
mkdir -p /tmp/alpine-tor/extracted
cd /tmp/alpine-tor/extracted
for apk in /tmp/alpine-tor/*.apk; do
    echo "Extracting $(basename $apk)..."
    tar xzf "$apk" 2>/dev/null || true
done

echo ""
echo "=== 4. Check the extracted Tor binary ==="
file usr/bin/tor 2>/dev/null
readelf -h usr/bin/tor 2>/dev/null | grep -E "Class|Machine|type"
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED
readelf -l usr/bin/tor 2>/dev/null | grep interpreter

echo ""
echo "=== 5. Check all deps are present ==="
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

echo ""
echo "=== 6. Copy musl-Tor + deps to packages/ ==="
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
mkdir -p "$REPO/packages/tor-musl-x86_64"
cp usr/bin/tor "$REPO/packages/tor-musl-x86_64/tor"
chmod +x "$REPO/packages/tor-musl-x86_64/tor"
# Copy all libs
mkdir -p "$REPO/packages/tor-musl-x86_64/lib"
cp -a lib/lib*.so* "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true
# Copy musl ld
cp lib/ld-musl-x86_64.so.1 "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null || true

echo "Vendored files:"
find "$REPO/packages/tor-musl-x86_64/" -type f | head -20
