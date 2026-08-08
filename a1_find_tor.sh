#!/bin/bash
# Download the actual Tor daemon from Alpine v3.20.
set +e
mkdir -p /tmp/alpine-tor3/extracted
cd /tmp/alpine-tor3

echo "=== 1. Download tor-0.4.8.x (the actual Tor daemon, not the browser) ==="
# The "tor" package in Alpine community IS the Tor daemon
# v3.19 has tor-0.4.8.13-r0.apk, v3.20 has tor-0.4.8.14-r0.apk
# But the listing showed "tor-24.02.2-r0.apk" — that might be a different tor
# Let me check what the actual filename is
echo "--- Checking exact filenames ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/" 2>&1 | grep -oP 'tor-0\.\d+\.\d+[^"]*\.apk' | head -5
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/" 2>&1 | grep -oP 'tor-0\.\d+\.\d+[^"]*\.apk' | head -5

echo ""
echo "=== 2. Download the correct package ==="
# Try the exact URL from v3.19
timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.4.8.13-r0.apk" -o tor.apk
ls -la tor.apk
file tor.apk
# If HTML, the URL is wrong — try with -v to see redirect
if file tor.apk | grep -q "HTML"; then
    echo "--- Got HTML, checking redirect ---"
    timeout 10 curl -sI "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.4.8.13-r0.apk" 2>&1 | head -10
    echo "--- Try v3.20 ---"
    timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/tor-0.4.8.14-r0.apk" -o tor.apk
    ls -la tor.apk
    file tor.apk
fi

echo ""
echo "=== 3. If still HTML, download from the minirootfs instead ==="
if file tor.apk | grep -q "HTML"; then
    echo "--- Extracting Tor from Alpine minirootfs ---"
    # We already downloaded alpine-minirootfs-3.19.7-x86_64.tar.gz
    if [ -f /tmp/alpine-minirootfs.tar.gz ]; then
        echo "Using minirootfs..."
        mkdir -p /tmp/alpine-rootfs
        cd /tmp/alpine-rootfs
        tar xzf /tmp/alpine-minirootfs.tar.gz 2>/dev/null
        # Install tor via apk add (needs network)
        echo "--- Installing tor via apk ---"
        apk add tor 2>/dev/null || echo "apk not available in WSL"
        # Check if tor is in the minirootfs already
        find . -name "tor" -type f 2>/dev/null | head -5
    fi
fi

echo ""
echo "=== 4. Alternative: use the minirootfs + chroot to install tor ==="
if [ -f /tmp/alpine-minirootfs.tar.gz ]; then
    echo "--- Set up Alpine chroot ---"
    mkdir -p /tmp/alpine-chroot
    cd /tmp/alpine-chroot
    tar xzf /tmp/alpine-minirootfs.tar.gz 2>/dev/null
    # Copy resolv.conf for network
    cp /etc/resolv.conf etc/resolv.conf 2>/dev/null || true
    # Chroot and install tor
    echo "--- Chroot + apk add tor ---"
    chroot . /bin/sh -c "apk update && apk add tor" 2>&1 | tail -10
    # Check if tor was installed
    file usr/bin/tor 2>/dev/null
    readelf -h usr/bin/tor 2>/dev/null | grep -E "Class|Machine"
fi
