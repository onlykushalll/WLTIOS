#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════
# build-terminal-wayland.sh — Build the Wayland terminal + DRM compositor
#
# Compiles wlt-term-wayland.c + compositor.c + comp_main.c and installs
# them into the rootfs. Run this AFTER build-initramfs64.sh (which creates
# the base rootfs), then repack the initramfs.
#
# Usage (from WSL2):
#   bash /mnt/c/.../WLTIOS/scripts/build-terminal-wayland.sh
# ═══════════════════════════════════════════════════════════════════════
set -e

REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
SRC=$REPO/src/userspace
COMP=$SRC/wlt-compositor
TERM=$SRC/wlt-terminal
ROOTFS=~/wltios-build/wltios-rootfs64

echo "=== STEP 1: Generate xdg-shell client protocol ==="
wayland-scanner client-header \
    /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
    /tmp/xdg-shell-client-protocol.h
wayland-scanner private-code \
    /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
    /tmp/xdg-shell-client-protocol.c
echo "xdg-shell protocol generated"

echo ""
echo "=== STEP 2: Compile Wayland terminal (wlt-term-wayland.c) ==="
gcc -O2 -Wall -I /tmp \
    $(pkg-config --cflags wayland-client) \
    $TERM/wlt-term-wayland.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -lutil \
    -o /tmp/wlt-terminal-wayland
echo "terminal compile exit: $?"
cp /tmp/wlt-terminal-wayland $ROOTFS/usr/bin/wlt-terminal
chmod 755 $ROOTFS/usr/bin/wlt-terminal
echo "terminal installed"

echo ""
echo "=== STEP 3: Compile DRM compositor (compositor.c + comp_main.c) ==="
cp $COMP/include/compositor.h /tmp/compositor.h
gcc -DWLR_USE_UNSTABLE -I /tmp -I $COMP \
    $(pkg-config --cflags wlroots-0.18 wayland-server libdrm pixman-1) \
    $COMP/compositor.c $COMP/comp_main.c \
    $(pkg-config --libs wlroots-0.18 wayland-server libdrm pixman-1) \
    -o /tmp/wlt-compositor
echo "compositor compile exit: $?"
cp /tmp/wlt-compositor $ROOTFS/usr/bin/wlt-compositor
chmod 755 $ROOTFS/usr/bin/wlt-compositor
echo "compositor installed"

echo ""
echo "=== STEP 4: Verify binaries ==="
file $ROOTFS/usr/bin/wlt-terminal $ROOTFS/usr/bin/wlt-compositor
ls -la $ROOTFS/usr/bin/wlt-terminal $ROOTFS/usr/bin/wlt-compositor

echo ""
echo "=== STEP 5: Repack initramfs (with fakeroot for dev nodes) ==="
fakeroot bash -c '
cd ~/wltios-build/wltios-rootfs64
rm -f dev/console dev/null dev/zero dev/mem dev/urandom dev/tty dev/ttyS0 dev/fb0
mkdir -p dev/dri
mknod -m 600 dev/console c 5 1
mknod -m 666 dev/null c 1 3
mknod -m 666 dev/zero c 1 5
mknod -m 640 dev/mem c 1 1
mknod -m 666 dev/urandom c 1 9
mknod -m 666 dev/tty c 5 0
mknod -m 660 dev/ttyS0 c 4 64
mknod -m 660 dev/fb0 c 29 0
mknod -m 660 dev/dri/card0 c 226 0
mknod -m 660 dev/dri/renderD128 c 226 128
find . | cpio -o -H newc 2>/dev/null | gzip -9 > ~/wltios-build/wltios-initramfs64.cpio.gz
echo "REPACKED"
'
ls -la ~/wltios-build/wltios-initramfs64.cpio.gz
echo ""
echo "=== ALL DONE ==="
echo "Boot with: bash $REPO/scripts/boot-terminal-fix.sh"
