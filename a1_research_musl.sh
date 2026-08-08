#!/bin/bash
# A.1-A.3: Research existing musl-Tor options + set up cross-compilation.
echo "=== 1. Check existing Alpine Tor (was 32-bit i386) ==="
file /home/kushal/wltios-build/tor-libs-alpine/tor 2>/dev/null
readelf -h /home/kushal/wltios-build/tor-libs-alpine/tor 2>/dev/null | grep -E "Class|Machine|type"
readelf -d /home/kushal/wltios-build/tor-libs-alpine/tor 2>/dev/null | grep NEEDED | head -15
readelf -l /home/kushal/wltios-build/tor-libs-alpine/tor 2>/dev/null | grep interpreter

echo ""
echo "=== 2. Check if musl-gcc is available ==="
which musl-gcc x86_64-linux-musl-gcc 2>/dev/null
dpkg -l 2>/dev/null | grep -i musl | head -5
find /usr -name "musl-gcc" -o -name "musl-ld" 2>/dev/null | head -5

echo ""
echo "=== 3. Check if we can install musl-tools ==="
apt-cache search musl 2>/dev/null | head -5

echo ""
echo "=== 4. Check if Alpine has a 64-bit Tor package we can download ==="
# Alpine packages are at dl-cdn.alpinelinux.org — we can download the x86_64 tor package
# which is musl-compiled 64-bit
echo "Checking Alpine package repo for x86_64 tor..."
timeout 10 curl -sI "https://dl-cdn.alpinelinux.org/alpine/latest-stable/main/x86_64/" 2>&1 | head -3

echo ""
echo "=== 5. Check the Alpine tor-libs-alpine directory for all files ==="
ls -la /home/kushal/wltios-build/tor-libs-alpine/ 2>/dev/null | head -20

echo ""
echo "=== 6. Check if there's a 64-bit Alpine rootfs we can extract Tor from ==="
find /home/kushal/wltios-build -name "alpine*" -type f 2>/dev/null | head -5
find /home/kushal/wltios-build -name "minirootfs*" 2>/dev/null | head -5

echo ""
echo "=== 7. Check if we can download Alpine minirootfs (x86_64, musl) ==="
# Alpine minirootfs is a full musl-based rootfs — we can extract tor from it
echo "Try downloading Alpine minirootfs x86_64..."
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/alpine-minirootfs-3.19.7-x86_64.tar.gz" -o /tmp/alpine-minirootfs.tar.gz 2>&1 | head -3
ls -la /tmp/alpine-minirootfs.tar.gz 2>/dev/null
