#!/bin/bash
cd /tmp && rm -rf check-inittab && mkdir check-inittab && cd check-inittab
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo "=== /etc/inittab ==="
cat etc/inittab 2>/dev/null
echo ""
echo "=== /sbin/init ==="
file sbin/init 2>/dev/null
ls -la sbin/init 2>/dev/null
echo ""
echo "=== /bin/sh ==="
ls -la bin/sh 2>/dev/null
file bin/sh 2>/dev/null
echo ""
echo "=== /bin/busybox ==="
file bin/busybox 2>/dev/null
echo ""
echo "=== grep for WLTIOS Boot in all files ==="
grep -r "WLTIOS Boot" . 2>/dev/null | head -5
echo ""
echo "=== grep for wlt-init in all files ==="
grep -r "wlt-init\|WLT-Shell\|WLT-Init" . 2>/dev/null | head -10
echo ""
echo "=== /etc/init.d/rcS ==="
cat etc/init.d/rcS 2>/dev/null | head -10
echo ""
echo "=== /etc/init.d/S90camkes_init ==="
cat etc/init.d/S90camkes_init 2>/dev/null
