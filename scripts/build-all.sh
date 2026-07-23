#!/bin/bash
set -e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
COMP=$REPO/src/userspace/wlt-compositor
TERM_DIR=$REPO/src/userspace/wlt-terminal
FM=$REPO/src/userspace/wlt-filemanager
LAUNCH=$REPO/src/userspace/wlt-launcher
SHELL=$REPO/src/userspace/wlt-shell
AI=$REPO/src/userspace/wlt-ai
TOR_MGR=$REPO/src/userspace/wlt-tor-mgr
SETTINGS=$REPO/src/userspace/wlt-settings
ROOTFS=~/wltios-build/wltios-rootfs64

echo "=== STEP 1: generate xdg-shell protocol ==="
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml /tmp/xdg-shell-client-protocol.h
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml /tmp/xdg-shell-client-protocol.c

echo "=== STEP 2: compile compositor ==="
cp $COMP/include/compositor.h /tmp/compositor.h
gcc -DWLR_USE_UNSTABLE -I /tmp -I $COMP \
    $(pkg-config --cflags wlroots-0.18 wayland-server libdrm pixman-1 xkbcommon) \
    $COMP/compositor.c $COMP/comp_main.c $COMP/blur.c \
    $(pkg-config --cflags glesv2 egl 2>/dev/null) \
    $(pkg-config --libs wlroots-0.18 wayland-server libdrm pixman-1 xkbcommon glesv2 egl 2>/dev/null) -lm \
    -o /tmp/wlt-compositor 2>&1
if [ $? -ne 0 ]; then echo "COMPOSITOR FAILED"; exit 1; fi
cp /tmp/wlt-compositor $ROOTFS/usr/bin/wlt-compositor && chmod 755 $ROOTFS/usr/bin/wlt-compositor
echo "compositor OK"

echo "=== STEP 3: compile terminal ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $TERM_DIR/wlt-term-wayland.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -lutil -o /tmp/wlt-terminal 2>&1
if [ $? -ne 0 ]; then echo "TERMINAL FAILED"; exit 1; fi
cp /tmp/wlt-terminal $ROOTFS/usr/bin/wlt-terminal && chmod 755 $ROOTFS/usr/bin/wlt-terminal
echo "terminal OK"

echo "=== STEP 4: compile file manager ==="
gcc -O2 -Wall -I $FM $(pkg-config --cflags wayland-client) \
    $FM/filemanager.c $FM/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-filemanager 2>&1
if [ $? -ne 0 ]; then echo "FILEMANAGER FAILED"; exit 1; fi
cp /tmp/wlt-filemanager $ROOTFS/usr/bin/wlt-filemanager && chmod 755 $ROOTFS/usr/bin/wlt-filemanager
echo "file manager OK"

echo "=== STEP 5: compile launcher ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $LAUNCH/wlt-launcher.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-launcher 2>&1
if [ $? -ne 0 ]; then echo "LAUNCHER FAILED"; exit 1; fi
cp /tmp/wlt-launcher $ROOTFS/usr/bin/wlt-launcher && chmod 755 $ROOTFS/usr/bin/wlt-launcher
echo "launcher OK"

echo "=== STEP 6: compile status bar ==="
gcc -O2 -Wall -I /tmp $(pkg-config --cflags wayland-client) \
    $SHELL/wlt-statusbar.c /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-statusbar 2>&1
if [ $? -ne 0 ]; then echo "STATUSBAR FAILED"; exit 1; fi
cp /tmp/wlt-statusbar $ROOTFS/usr/bin/wlt-statusbar && chmod 755 $ROOTFS/usr/bin/wlt-statusbar
echo "status bar OK"

echo "=== STEP 6.5: build libwlt-ai.a ==="
cd $AI
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "WLT-AI LIB FAILED"; exit 1; fi
cp libwlt-ai.a $ROOTFS/usr/lib/ 2>/dev/null || true
mkdir -p $ROOTFS/usr/include/wlt-ai
cp include/ai.h include/wlt_http.h $ROOTFS/usr/include/wlt-ai/ 2>/dev/null || true
cd $REPO
echo "libwlt-ai.a OK"

echo "=== STEP 7: compile kairos window (links libwlt-ai.a) ==="
gcc -O2 -Wall -I /tmp -I $AI/include $(pkg-config --cflags wayland-client) \
    $AI/kairos_window.c /tmp/xdg-shell-client-protocol.c \
    -L $AI -lwlt-ai -L $REPO/src/userspace/wlt-permissions -lwlt-permissions $(pkg-config --libs wayland-client) -o /tmp/kairos-window 2>&1
if [ $? -ne 0 ]; then echo "KAIROS FAILED"; exit 1; fi
cp /tmp/kairos-window $ROOTFS/usr/bin/kairos-window && chmod 755 $ROOTFS/usr/bin/kairos-window
echo "kairos window OK"

echo "=== STEP 8: compile settings app ==="
gcc -O2 -D_GNU_SOURCE -Wall -I /tmp -I "$SETTINGS/include" $(pkg-config --cflags wayland-client) \
    "$SETTINGS/wlt-settings-app.c" /tmp/xdg-shell-client-protocol.c \
    $(pkg-config --libs wayland-client) -o /tmp/wlt-settings 2>&1
if [ $? -ne 0 ]; then echo "settings FAILED (non-fatal)"; else
    cp /tmp/wlt-settings $ROOTFS/usr/bin/wlt-settings && chmod 755 $ROOTFS/usr/bin/wlt-settings
    echo "settings OK"
fi

echo "=== STEP 9: compile wlttor (Tor manager) ==="
cd $TOR_MGR
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "WLTTOR FAILED (non-fatal)"; else
    cp wlttor $ROOTFS/usr/bin/wlttor && chmod 755 $ROOTFS/usr/bin/wlttor
    echo "wlttor OK"
fi
cd $REPO

echo "=== STEP 9.3: build libwlt-pkg.a ==="
PKG=$REPO/src/userspace/wlt-pkg
cd $PKG
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "WLT-PKG FAILED (non-fatal)"; else
    cp libwlt-pkg.a $ROOTFS/usr/lib/ 2>/dev/null || true
    mkdir -p $ROOTFS/usr/include/wlt-pkg
    cp include/pkg.h $ROOTFS/usr/include/wlt-pkg/ 2>/dev/null || true
    echo "libwlt-pkg OK"
fi
cd $REPO

echo "=== STEP 9.5: build libwlt-permissions.a + libwlt-sandbox.a ==="
PERMS=$REPO/src/userspace/wlt-permissions
SANDBOX=$REPO/src/userspace/wlt-sandbox
cd $PERMS
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "PERMISSIONS FAILED"; exit 1; fi
cp libwlt-permissions.a $ROOTFS/usr/lib/ 2>/dev/null || true
cp libwlt-permissions.so $ROOTFS/usr/lib/ 2>/dev/null || true
mkdir -p $ROOTFS/usr/include/wlt-permissions
cp include/permissions.h include/manifest.h $ROOTFS/usr/include/wlt-permissions/ 2>/dev/null || true
echo "libwlt-permissions OK"
cd $SANDBOX
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "SANDBOX FAILED"; exit 1; fi
cp libwlt-sandbox.a $ROOTFS/usr/lib/ 2>/dev/null || true
cp libwlt-sandbox.so $ROOTFS/usr/lib/ 2>/dev/null || true
echo "libwlt-sandbox OK"
cd $REPO

echo "=== STEP 9.6: build libwlt-crypto.a (with luks.c) ==="
CRYPTO=$REPO/src/userspace/wlt-crypto
cd $CRYPTO
make clean 2>/dev/null || true
make 2>&1
if [ $? -ne 0 ]; then echo "WLT-CRYPTO FAILED"; exit 1; fi
cp libwlt-crypto.a $ROOTFS/usr/lib/ 2>/dev/null || true
cp libwlt-crypto.so $ROOTFS/usr/lib/ 2>/dev/null || true
mkdir -p $ROOTFS/usr/include/wlt-crypto
cp include/crypto.h include/keyvault.h include/file_crypto.h $ROOTFS/usr/include/wlt-crypto/ 2>/dev/null || true
echo "libwlt-crypto OK"
cd $REPO

echo "=== STEP 9.7: build libwlt-update.a (Ed25519, vendored libsodium) ==="
UPDATE=$REPO/src/userspace/wlt-update
SODIUM=$REPO/packages/sodium-libs
cd $UPDATE
make clean 2>/dev/null || true
# Prefer vendored libsodium so HAVE_SODIUM is defined and Ed25519 verification
# is compiled in; fall back to system pkg-config if the vendored copy is absent.
if [ -f "$SODIUM/libsodium.a" ] && [ -d "$SODIUM/include" ]; then
    # Vendored libsodium.a is not -fPIC, so it cannot link into the shared
    # lib (.so); build only the static lib (.a), which is what the rootfs
    # needs. Static .o files are compiled with -fPIC so they archive fine.
    make libwlt-update.a CPPFLAGS="-Iinclude -I$SODIUM/include -D_GNU_SOURCE -DHAVE_SODIUM" \
         LDLIBS="$SODIUM/libsodium.a" 2>&1
else
    make 2>&1
fi
if [ $? -ne 0 ]; then echo "WLT-UPDATE FAILED"; exit 1; fi
cp libwlt-update.a $ROOTFS/usr/lib/ 2>/dev/null || true
cp libwlt-update.so $ROOTFS/usr/lib/ 2>/dev/null || true
mkdir -p $ROOTFS/usr/include/wlt-update
cp include/update.h include/partition.h $ROOTFS/usr/include/wlt-update/ 2>/dev/null || true
echo "libwlt-update OK"
cd $REPO

echo "=== STEP 10: Install vendored Tor runtime libs ==="
TORLIBS=$REPO/packages/tor-libs
if [ -d "$TORLIBS" ]; then
    cp "$TORLIBS/liblzma.so.5" $ROOTFS/lib/
    cp "$TORLIBS/libzstd.so.1.5.7" $ROOTFS/lib/
    ln -sf libzstd.so.1.5.7 $ROOTFS/lib/libzstd.so.1
    cp "$TORLIBS/libevent-2.1.so.7.0.1" $ROOTFS/lib/
    ln -sf libevent-2.1.so.7.0.1 $ROOTFS/lib/libevent-2.1.so.7
    echo "Tor runtime libs installed from vendored packages/tor-libs/"
else
    echo "WARNING: packages/tor-libs/ not found — Tor may fail to start"
fi

echo "=== STEP 11: Ensure Tor control port is configured ==="
grep -q ControlPort $ROOTFS/etc/tor/torrc 2>/dev/null || echo "ControlPort 9051" >> $ROOTFS/etc/tor/torrc
grep -q CookieAuthentication $ROOTFS/etc/tor/torrc 2>/dev/null || echo "CookieAuthentication 1" >> $ROOTFS/etc/tor/torrc
echo "Tor control port configured"

echo "=== STEP 12: repack initramfs ==="
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

echo "=== STEP 13: Install wlroots/wayland runtime libs ==="
WLLIBS=$REPO/packages/wl-libs
if [ -d "$WLLIBS" ]; then
    cp "$WLLIBS"/*.so* $ROOTFS/lib/ 2>/dev/null || true
    # Copy ld-linux
    cp "$WLLIBS"/ld-linux-x86-64.so.2 $ROOTFS/lib64/ 2>/dev/null || true
    ln -sf /lib64/ld-linux-x86-64.so.2 $ROOTFS/lib/ld-linux-x86-64.so.2 2>/dev/null || true
    echo "wlroots/wayland runtime libs installed from packages/wl-libs/"
else
    echo "WARNING: packages/wl-libs/ not found"
fi

echo "=== STEP 14: Final repack ==="
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
echo "FINAL REPACK DONE"
'
