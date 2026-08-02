#!/bin/bash
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64

echo "=== Base rootfs lib/ ==="
cd /tmp && rm -rf check-libs && mkdir check-libs && cd check-libs
cpio -idm < "$BASE" 2>/dev/null
ls -la lib/ld-* lib/libc* lib64/ld-* lib64/libc* 2>/dev/null | head -20
echo ""
echo "=== wltios-rootfs64 lib/ ==="
ls -la "$TOR_SRC/lib/ld-"* "$TOR_SRC/lib/libc"* "$TOR_SRC/lib64/ld-"* "$TOR_SRC/lib64/libc"* 2>/dev/null | head -20
echo ""
echo "=== WSL system glibc ld ==="
ls -la /lib64/ld-linux-x86-64.so.2 /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 2>/dev/null
file /lib64/ld-linux-x86-64.so.2 2>/dev/null
echo ""
echo "=== Tor binary interpreter ==="
readelf -l "$TOR_SRC/usr/bin/tor" 2>/dev/null | grep interpreter
echo ""
echo "=== Tor NEEDED ==="
readelf -d "$TOR_SRC/usr/bin/tor" 2>/dev/null | grep NEEDED
echo ""
echo "=== Check if Tor is musl or glibc ==="
strings "$TOR_SRC/usr/bin/tor" | grep -c "musl\|GLIBC"
strings "$TOR_SRC/usr/bin/tor" | grep "musl\|GLIBC" | head -5
