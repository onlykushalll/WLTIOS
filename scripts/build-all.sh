#!/bin/bash
set -e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
COMP=$REPO/src/userspace/wlt-compositor
TERM_DIR=$REPO/src/userspace/wlt-terminal
FM=$REPO/src/userspace/wlt-filemanager
LAUNCH=$REPO/src/userspace/wlt-launcher
SHELL=$REPO/src/userspace/wlt-shell
AI=$REPO/src/userspace/wlt-ai
SETTINGS=$REPO/src/userspace/wlt-settings
ROOTFS=~/wltios-build/wltios-rootfs64

echo "=== STEP 1: generate xdg-shell protocol ==="
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml /tmp/xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml /tmp/xdg-shell-client-protocol.c

echo "=== STEP 2: compile compositor ==="
cp $COMP/include/compositor.h /tmp/compositor.h
gcc -DWLR_USE_UNSTABLE -I /tmp -I $COMP \
    $(pkg-config --cflags wlroots-0.18 wayland-server libdrm pixman-1 xkbcommon) \
    $COMP/compositor.c $COMP/comp_main.c \
    $(pkg-config --libs wlroots-0.18 wayland-server libdrm pixman-1 xkbcommon) \
    -o /tmp/wlt-compositor 2>&1 | tail -3
cp /tmp/wlt-compositor $ROOTFS/usr/bin/wlt-compositor && chmod 755 $ROOTFS/usr/bin/wlt-compositor

echo "=== STEP 3: compile terminal ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $TERM_DIR/wlt-term-wayland.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -lutil -o /tmp/wlt-terminal 2>&1 | tail -3
cp /tmp/wlt-terminal $ROOTFS/usr/bin/wlt-terminal && chmod 755 $ROOTFS/usr/bin/wlt-terminal

echo "=== STEP 4: compile file manager ==="
gcc -O2 -Wall -I $FM $(pkg-config --cflags wayland-client) \
    $FM/filemanager.c $FM/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-filemanager 2>&1 | tail -3
cp /tmp/wlt-filemanager $ROOTFS/usr/bin/wlt-filemanager && chmod 755 $ROOTFS/usr/bin/wlt-filemanager

echo "=== STEP 5: compile launcher ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $LAUNCH/wlt-launcher.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-launcher 2>&1 | tail -3
cp /tmp/wlt-launcher $ROOTFS/usr/bin/wlt-launcher && chmod 755 $ROOTFS/usr/bin/wlt-launcher

echo "=== STEP 6: compile status bar ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $SHELL/wlt-statusbar.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-statusbar 2>&1 | tail -3
cp /tmp/wlt-statusbar $ROOTFS/usr/bin/wlt-statusbar && chmod 755 $ROOTFS/usr/bin/wlt-statusbar

echo "=== STEP 7: compile kairos window ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $AI/kairos_window.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/kairos-window 2>&1 | tail -3
cp /tmp/kairos-window $ROOTFS/usr/bin/kairos-window && chmod 755 $ROOTFS/usr/bin/kairos-window

echo "=== STEP 8: try settings app ==="
gcc -O2 -Wall -I $SETTINGS -I /tmp $(pkg-config --cflags wayland-client) \
    $SETTINGS/wlt-settings-app.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-settings 2>&1 | tail -5
if [ -f /tmp/wlt-settings ]; then
    cp /tmp/wlt-settings $ROOTFS/usr/bin/wlt-settings && chmod 755 $ROOTFS/usr/bin/wlt-settings
    echo "settings app compiled OK"
else
    echo "settings app compile FAILED (expected — has include deps)"
fi

echo "=== STEP 9: repack initramfs ==="
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
echo "=== ALL DONE ==="
