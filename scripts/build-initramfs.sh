#!/bin/bash
set -e
WLTIOS_SRC="/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src"
OUT_DIR="$HOME/wltios-build/wltios-rootfs"
BUILD_DIR="$HOME/wltios-build/wltios-compile"
TOR_LIBS="$HOME/wltios-build/tor-libs-alpine"

rm -rf "$OUT_DIR" "$BUILD_DIR"
mkdir -p "$OUT_DIR"/{bin,sbin,lib,lib64,proc,sys,dev,tmp,var/log,var/lib/tor,etc/tor,etc/wlt,run,root,home,usr/bin,usr/sbin,usr/lib,usr/share/tor}
mkdir -p "$BUILD_DIR"

CFLAGS="-m32 -static -Wall -Wextra -Werror -std=gnu11 -O0 -fno-merge-constants -fno-merge-all-constants"
COMMON_INC="-include stdint.h -include string.h -include stdlib.h -include stdio.h -include unistd.h -include errno.h -include fcntl.h -D_GNU_SOURCE"

echo "=== Building WLTIOS Initramfs (real modules + QEMU net + Tor) ==="

# 1. init-minimal (v3 — starts wlt-net + Tor + shell)
gcc $CFLAGS $COMMON_INC -include sys/wait.h -include sys/mount.h -include sys/reboot.h -include linux/reboot.h \
    "${WLTIOS_SRC}/userspace/wlt-init/init-minimal.c" -o "$BUILD_DIR/wlt-init-minimal" 2>&1
echo "  init OK"

# 2. wlt-shell (with netcheck + torcheck + tstatus commands)
gcc $CFLAGS $COMMON_INC \
    -include sys/socket.h -include netinet/in.h -include arpa/inet.h \
    -include sys/ioctl.h -include net/if.h -include netdb.h \
    "${WLTIOS_SRC}/userspace/wlt-shell/cmds/shell.c" \
    -I"${WLTIOS_SRC}/userspace/wlt-shell/include" -o "$BUILD_DIR/wlt-shell" 2>&1
echo "  shell OK (with netcheck + torcheck + tstatus)"

# 3. wlt-net (REAL QEMU bringup — qemu_bringup.c using raw ioctl + lo + eth0)
cat > /tmp/wlt-net-svc.c << 'NETEOF'
#include "net.h"
extern int wlt_net_qemu_bringup(void);
int main(void) {
    int rc = wlt_net_qemu_bringup();
    if (rc != 0) {
        write(1, "[wlt-net] QEMU bringup FAILED\r\n", 31);
        return 1;
    }
    write(1, "[wlt-net] Network service initialized (QEMU mode)\r\n", 51);
    return 0;
}
NETEOF
NET_DIR="${WLTIOS_SRC}/userspace/wlt-net"
gcc $CFLAGS $COMMON_INC -DWLT_TESTING \
    /tmp/wlt-net-svc.c \
    ${NET_DIR}/qemu_bringup.c ${NET_DIR}/ethernet.c ${NET_DIR}/config.c \
    ${NET_DIR}/tor_proxy.c ${NET_DIR}/kill_switch.c ${NET_DIR}/mac.c ${NET_DIR}/wifi.c ${NET_DIR}/net.c \
    -I"$NET_DIR/include" \
    -include sys/socket.h -include netinet/in.h -include arpa/inet.h \
    -include sys/ioctl.h -include net/if.h -include linux/route.h \
    -o "$BUILD_DIR/wlt-net" 2>&1
echo "  wlt-net OK (real QEMU bringup with loopback + eth0)"

# 4. wlt-crypto (REAL source)
cat > /tmp/wlt-crypto-svc.c << 'CRYPEOF'
#include "crypto.h"
#include "keyvault.h"
int main(void) {
    wlt_crypto_error_t err = wlt_crypto_init();
    if (err != WLT_CRYPTO_OK) return 1;
    write(1, "[wlt-crypto] Crypto initialized\r\n", 33);
    wlt_keyvault_init();
    wlt_crypto_shutdown();
    return 0;
}
CRYPEOF
CRYPTO_DIR="${WLTIOS_SRC}/userspace/wlt-crypto"
CRYPTO_SRCS=$(find "$CRYPTO_DIR" -maxdepth 1 -name "*.c" ! -name "*_test*" 2>/dev/null | tr '\n' ' ')
gcc $CFLAGS $COMMON_INC /tmp/wlt-crypto-svc.c $CRYPTO_SRCS -I"$CRYPTO_DIR/include" -o "$BUILD_DIR/wlt-crypto" 2>&1
echo "  wlt-crypto OK (real source)"

# 5. Install binaries
cp "$BUILD_DIR/wlt-init-minimal" "$OUT_DIR/sbin/init"
cp "$BUILD_DIR/wlt-shell" "$OUT_DIR/bin/wlt-shell"
cp "$BUILD_DIR/wlt-net" "$OUT_DIR/sbin/wlt-net"
cp "$BUILD_DIR/wlt-crypto" "$OUT_DIR/sbin/wlt-crypto"
echo '#!/bin/sh' > "$OUT_DIR/init"; echo 'exec /sbin/init' >> "$OUT_DIR/init"; chmod 755 "$OUT_DIR/init"
echo "WLTIOS_VERSION=0.1.0" > "$OUT_DIR/etc/wltios-release"
echo "wltios" > "$OUT_DIR/etc/hostname"
echo "nameserver 10.0.2.3" > "$OUT_DIR/etc/resolv.conf"

# /etc/passwd + /etc/group (needed by Tor to resolve uid 0)
echo "root:x:0:0:root:/root:/bin/sh" > "$OUT_DIR/etc/passwd"
echo "root:x:0:" > "$OUT_DIR/etc/group"

# Device nodes
mknod "$OUT_DIR/dev/console" c 5 1 2>/dev/null || true
mknod "$OUT_DIR/dev/null" c 1 3 2>/dev/null || true
mknod "$OUT_DIR/dev/ttyS0" c 4 64 2>/dev/null || true
mknod "$OUT_DIR/dev/urandom" c 1 9 2>/dev/null || true

# 6. Install Tor binary + Alpine musl libs
if [ -x "$TOR_LIBS/tor" ]; then
    cp "$TOR_LIBS/tor" "$OUT_DIR/usr/bin/tor"
    chmod 755 "$OUT_DIR/usr/bin/tor"
    cp "$TOR_LIBS/share/tor/geoip" "$OUT_DIR/usr/share/tor/geoip" 2>/dev/null || true
    cp "$TOR_LIBS/share/tor/geoip6" "$OUT_DIR/usr/share/tor/geoip6" 2>/dev/null || true

    # Install musl loader + shared libs to /lib
    cp "$TOR_LIBS"/ld-musl-i386.so.1 "$OUT_DIR/lib/"
    cp "$TOR_LIBS"/lib*.so* "$OUT_DIR/lib/" 2>/dev/null || true
    # Create musl libc symlink
    cd "$OUT_DIR/lib"
    ln -sf ld-musl-i386.so.1 libc.musl-x86.so.1
    # Create version symlinks for all libs
    for f in libz.so.1.3.2; do ln -sf "$f" "${f%.3.2}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    for f in libevent-2.1.so.7.0.1; do ln -sf "$f" "${f%.0.1}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    for f in liblzma.so.5.8.3; do ln -sf "$f" "${f%.8.3}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    for f in libzstd.so.1.5.6; do ln -sf "$f" "${f%.5.6}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    for f in libseccomp.so.2.5.5; do ln -sf "$f" "${f%.5.5}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    for f in libcap.so.2.78; do ln -sf "$f" "${f%.78}" 2>/dev/null; ln -sf "$f" "${f%.*}" 2>/dev/null; done
    cd -

    # Tor data directory
    chmod 700 "$OUT_DIR/var/lib/tor"

    # torrc
    cat > "$OUT_DIR/etc/tor/torrc" << 'TORRC'
DataDirectory /var/lib/tor
Log notice stderr
SocksPort 9050
ExitPolicy reject *:*
GeoIPFile /usr/share/tor/geoip
GeoIPv6File /usr/share/tor/geoip6
User root
TORRC
    echo "  tor installed ($($TOR_LIBS/ld-musl-i386.so.1 --library-path $TOR_LIBS $TOR_LIBS/tor --version 2>&1 | head -1))"
else
    echo "  tor: NOT FOUND at $TOR_LIBS/tor (skip — network-only mode)"
fi

# 7. Pack — use --owner=root:root so /var/lib/tor is owned by root in the initramfs
cd "$OUT_DIR"
find . | cpio --create --format=newc --owner=root:root | gzip > "$HOME/wltios-build/wltios-initramfs.cpio.gz"
ls -lh "$HOME/wltios-build/wltios-initramfs.cpio.gz"
echo "=== Initramfs build complete ==="