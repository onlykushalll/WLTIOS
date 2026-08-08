#!/bin/bash
# Find correct Alpine tor package URL + download.
set +e
mkdir -p /tmp/alpine-tor2/extracted
cd /tmp/alpine-tor2

echo "=== 1. Check what's at the Alpine community x86_64 repo ==="
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/" 2>&1 | grep -o 'tor[^"]*\.apk' | head -10

echo ""
echo "=== 2. Try v3.20 ==="
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/" 2>&1 | grep -o 'tor[^"]*\.apk' | head -10

echo ""
echo "=== 3. Try edge ==="
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/edge/community/x86_64/" 2>&1 | grep -o 'tor[^"]*\.apk' | head -10

echo ""
echo "=== 4. Download from v3.20 ==="
timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/tor-0.4.8.14-r0.apk" -o tor.apk 2>&1
ls -la tor.apk
file tor.apk

echo ""
echo "=== 5. If that fails, try listing exact filename ==="
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/" 2>&1 | grep -o 'tor-[^"]*\.apk' | head -5
