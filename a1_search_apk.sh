#!/bin/bash
# Use Alpine package search API to find the correct Tor package.
set +e

echo "=== 1. Search Alpine package index for tor ==="
timeout 15 curl -sL "https://pkgs.alpinelinux.org/packages?name=tor&branch=v3.20&arch=x86_64" 2>&1 | grep -oP 'tor[^<]*' | head -20

echo ""
echo "=== 2. Try the APK index file ==="
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/APKINDEX.tar.gz" -o /tmp/APKINDEX.tar.gz 2>&1
ls -la /tmp/APKINDEX.tar.gz
cd /tmp && tar xzf APKINDEX.tar.gz 2>/dev/null
grep "^P:tor$" APKINDEX 2>/dev/null
grep -A5 "^P:tor$" APKINDEX 2>/dev/null | head -10

echo ""
echo "=== 3. Try directly with the package name from the listing ==="
# The listing showed "tor-0.9.0-r10.apk" — but that might be a different "tor" package
# (Alpine has a terminal multiplexer called "tor" too? No, that's "tmux")
# Let me check what "tor" package provides /usr/bin/tor
timeout 15 curl -sL "https://pkgs.alpinelinux.org/contents?name=tor&path=/usr/bin/tor&arch=x86_64&branch=v3.20" 2>&1 | grep -oP 'tor[^<]*' | head -10

echo ""
echo "=== 4. Actually download the file that the listing shows ==="
# The HTML listing showed tor-0.9.0-r10.apk — let me try that exact filename
timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.9.0-r10.apk" -o /tmp/tor-test.apk 2>&1
ls -la /tmp/tor-test.apk
file /tmp/tor-test.apk
