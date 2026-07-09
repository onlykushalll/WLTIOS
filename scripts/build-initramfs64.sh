#!/bin/bash
set +e
LOG=/mnt/c/Users/Default.L-HCG-9FVVGS3/AppData/Local/Temp/wlt-build64.log
WLTIOS_SRC="/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src"
OUT_DIR="$HOME/wltios-build/wltios-rootfs64"
BUILD_DIR="$HOME/wltios-build/wltios-compile64"

echo [[BUILD64 START]] > $LOG
date >> $LOG

rm -rf "$OUT_DIR" "$BUILD_DIR"
mkdir -p "$OUT_DIR"/{bin,sbin,lib,lib64,proc,sys,dev,tmp,var/log,var/lib/tor,etc/tor,etc/wlt,run,root,home,usr/bin,usr/sbin,usr/lib,usr/share/tor}
mkdir -p "$BUILD_DIR"

CFLAGS="-m64 -static -Wall -Wextra -Werror -std=gnu11 -O0"
COMMON_INC="-include stdint.h -include string.h -include stdlib.h -include stdio.h -include unistd.h -include errno.h -include fcntl.h -D_GNU_SOURCE"

echo "=== Building 64-bit init ===" >> $LOG
gcc $CFLAGS $COMMON_INC -include sys/wait.h -include sys/mount.h -include sys/stat.h \
    -include sys/reboot.h -include linux/reboot.h -include linux/fb.h -include sys/ioctl.h \
    -include sys/syscall.h \
    "${WLTIOS_SRC}/userspace/wlt-init/init-minimal.c" -o "$BUILD_DIR/wlt-init-minimal" 2>> $LOG
echo "init: $?" >> $LOG

echo "=== Building 64-bit shell ===" >> $LOG
gcc $CFLAGS $COMMON_INC \
    -include sys/socket.h -include netinet/in.h -include arpa/inet.h \
    -include sys/ioctl.h -include net/if.h -include netdb.h \
    -include sys/mman.h -include linux/fb.h -include sched.h \
    "${WLTIOS_SRC}/userspace/wlt-shell/cmds/shell.c" \
    -I"${WLTIOS_SRC}/userspace/wlt-shell/include" -o "$BUILD_DIR/wlt-shell" 2>> $LOG
echo "shell: $?" >> $LOG

echo "=== Building 64-bit wlt-net ===" >> $LOG
cat > /tmp/wlt-net-svc64.c << 'NETEOF'
#include "net.h"
extern int wlt_net_qemu_bringup(void);
int main(void) {
    int rc = wlt_net_qemu_bringup();
    if (rc != 0) return 1;
    write(1, "[wlt-net] Network service initialized (QEMU mode)\r\n", 51);
    return 0;
}
NETEOF
NET_DIR="${WLTIOS_SRC}/userspace/wlt-net"
gcc $CFLAGS $COMMON_INC -DWLT_TESTING \
    /tmp/wlt-net-svc64.c \
    ${NET_DIR}/qemu_bringup.c ${NET_DIR}/ethernet.c ${NET_DIR}/config.c \
    ${NET_DIR}/tor_proxy.c ${NET_DIR}/kill_switch.c ${NET_DIR}/mac.c ${NET_DIR}/wifi.c ${NET_DIR}/net.c \
    -I"$NET_DIR/include" \
    -include sys/socket.h -include netinet/in.h -include arpa/inet.h \
    -include sys/ioctl.h -include net/if.h -include net/if_arp.h -include linux/route.h \
    -o "$BUILD_DIR/wlt-net" 2>> $LOG
echo "wlt-net: $?" >> $LOG

echo "=== Building 64-bit wlt-crypto ===" >> $LOG
cat > /tmp/wlt-crypto-svc64.c << 'CRYPEOF'
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
gcc $CFLAGS $COMMON_INC /tmp/wlt-crypto-svc64.c $CRYPTO_SRCS -I"$CRYPTO_DIR/include" -o "$BUILD_DIR/wlt-crypto" 2>> $LOG
echo "wlt-crypto: $?" >> $LOG

# Install static binaries
cp "$BUILD_DIR/wlt-init-minimal" "$OUT_DIR/sbin/init"
cp "$BUILD_DIR/wlt-shell" "$OUT_DIR/bin/wlt-shell"
cp "$BUILD_DIR/wlt-net" "$OUT_DIR/sbin/wlt-net"
cp "$BUILD_DIR/wlt-crypto" "$OUT_DIR/sbin/wlt-crypto"

# === WLTIOS Defender (Guardian) bundle ===
echo "=== Installing Guardian Defender ===" >> $LOG
GUARDIAN_SRC="${WLTIOS_SRC}/userspace/wlt-defender"
if [ -d "$GUARDIAN_SRC" ] && [ -x "$GUARDIAN_SRC/build/guardian" ]; then
    INITRAMFS_DIR="$OUT_DIR" GUARDIAN_SRC="$GUARDIAN_SRC" \
        bash "$GUARDIAN_SRC/wlt-integration/build-initramfs-snippet.sh" 2>> $LOG
    echo "guardian bundle: $?" >> $LOG

    # Copy shared libs (guardian is dynamically linked)
    mkdir -p "$OUT_DIR/etc/wlt/guardian/lib"
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libyara.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libbpf.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libelf.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libcrypto.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libsqlite3.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libzstd.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    cp -f "$GUARDIAN_SRC/local-deps/usr/lib/x86_64-linux-gnu/libjitterentropy.so"* "$OUT_DIR/etc/wlt/guardian/lib/" 2>/dev/null || true
    echo "guardian shared libs: $?" >> $LOG
else
    echo "guardian: source or binary not found, skipping" >> $LOG
fi

# Standard files
echo '#!/bin/sh' > "$OUT_DIR/init"; echo 'exec /sbin/init' >> "$OUT_DIR/init"; chmod 755 "$OUT_DIR/init"
echo "WLTIOS_VERSION=0.1.0" > "$OUT_DIR/etc/wltios-release"
echo "wltios" > "$OUT_DIR/etc/hostname"
echo "nameserver 10.0.2.3" > "$OUT_DIR/etc/resolv.conf"
echo "root:x:0:0:root:/root:/bin/sh" > "$OUT_DIR/etc/passwd"
echo "root:x:0:" > "$OUT_DIR/etc/group"

# Device nodes
mknod "$OUT_DIR/dev/console" c 5 1 2>/dev/null || true
mknod "$OUT_DIR/dev/null" c 1 3 2>/dev/null || true
mknod "$OUT_DIR/dev/ttyS0" c 4 64 2>/dev/null || true
mknod "$OUT_DIR/dev/urandom" c 1 9 2>/dev/null || true
mknod "$OUT_DIR/dev/mem" c 1 1 2>/dev/null || true

echo "=== Extracting e1000.ko for 64-bit ===" >> $LOG
mkdir -p /tmp/amd64-extract
dpkg-deb -x /tmp/debian-amd64-kernel.deb /tmp/amd64-extract 2>/dev/null
E1000_KO=$(find /tmp/amd64-extract -name "e1000.ko" 2>/dev/null | head -1)
if [ -n "$E1000_KO" ]; then
    mkdir -p "$OUT_DIR/lib/modules"
    cp "$E1000_KO" "$OUT_DIR/lib/modules/e1000.ko"
    echo "e1000.ko copied: $E1000_KO" >> $LOG
else
    echo "e1000.ko not found — checking if built-in" >> $LOG
    grep -i e1000 /tmp/amd64-extract/lib/modules/5.10.0-30-amd64/modules.builtin 2>/dev/null >> $LOG
fi

echo "=== Installing compositor + wlroots libs ===" >> $LOG
COMP_DIR="${WLTIOS_SRC}/userspace/wlt-compositor"
# Rebuild compositor with full paths
gcc -Wall -Wextra -DWLR_USE_UNSTABLE -I"${COMP_DIR}" -I"${COMP_DIR}/include" \
    $(pkg-config --cflags wlroots-0.18 wayland-server) \
    "${COMP_DIR}/compositor.c" "${COMP_DIR}/comp_main.c" \
    $(pkg-config --libs wlroots-0.18 wayland-server) \
    -o /tmp/wlt-compositor-test 2>> $LOG
if [ -x /tmp/wlt-compositor-test ]; then
    cp /tmp/wlt-compositor-test "$OUT_DIR/usr/bin/wlt-compositor"
    chmod 755 "$OUT_DIR/usr/bin/wlt-compositor"
    for lib in $(ldd /tmp/wlt-compositor-test 2>/dev/null | awk '{print $3}' | grep "^/"); do
        cp -L "$lib" "$OUT_DIR/lib/" 2>/dev/null || true
    done
    cp /lib64/ld-linux-x86-64.so.2 "$OUT_DIR/lib64/" 2>/dev/null || true
    ln -sf /lib64/ld-linux-x86-64.so.2 "$OUT_DIR/lib/ld-linux-x86-64.so.2" 2>/dev/null || true
    LIB_COUNT=$(ls "$OUT_DIR/lib/"*.so* 2>/dev/null | wc -l)
    echo "compositor + $LIB_COUNT libs copied" >> $LOG

    # DRM/Mesa driver: NOT caught by the ldd loop above, because it's loaded
    # via dlopen() at runtime based on detected hardware, not linked at
    # compile time. Bundling it explicitly, at the exact path Mesa searches
    # by default on this system, and also exporting LIBGL_DRIVERS_PATH as a
    # belt-and-suspenders fallback in case the initramfs's directory layout
    # doesn't line up with Mesa's compiled-in default search path.
    echo "=== Installing Mesa DRI driver (dlopen-time, not caught by ldd) ===" >> $LOG
    mkdir -p "$OUT_DIR/usr/lib/x86_64-linux-gnu/dri"
    DRIL_REAL=$(readlink -f /usr/lib/x86_64-linux-gnu/dri/virtio_gpu_dri.so 2>/dev/null)
    if [ -n "$DRIL_REAL" ] && [ -f "$DRIL_REAL" ]; then
        cp -L "$DRIL_REAL" "$OUT_DIR/usr/lib/x86_64-linux-gnu/dri/libdril_dri.so"
        for alias in virtio_gpu_dri.so swrast_dri.so kms_swrast_dri.so; do
            ln -sf libdril_dri.so "$OUT_DIR/usr/lib/x86_64-linux-gnu/dri/$alias"
        done
        # libdril_dri.so's own deps (checked directly, not assumed):
        # libgbm libm libc libdrm libexpat libgcc_s — all already covered
        # by the ldd loop above except libgcc_s, which wasn't a direct
        # compositor dependency. Bundle it explicitly too.
        cp -L /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 "$OUT_DIR/lib/" 2>/dev/null || true
        echo "libdril_dri.so + aliases installed ($(stat -c%s "$OUT_DIR/usr/lib/x86_64-linux-gnu/dri/libdril_dri.so" 2>/dev/null) bytes)" >> $LOG
    else
        echo "WARNING: libdril_dri.so not found on build host — DRM backend will fail to load a GPU driver even if /dev/dri/card0 exists in the guest" >> $LOG
    fi
else
    echo "compositor rebuild failed" >> $LOG
fi

echo "=== Installing 64-bit Tor ===" >> $LOG
cd /tmp
if [ ! -f /tmp/tor-amd64.apk ]; then
    curl -s --max-time 60 -L -o /tmp/tor-amd64.apk "https://dl-cdn.alpinelinux.org/alpine/v3.20/community/x86_64/tor-0.4.9.9-r0.apk" 2>> $LOG
fi
if [ -s /tmp/tor-amd64.apk ]; then
    mkdir -p /tmp/tor-amd64-extract
    tar -xzf /tmp/tor-amd64.apk -C /tmp/tor-amd64-extract 2>/dev/null
    TOR_BIN=$(find /tmp/tor-amd64-extract -name "tor" -type f -executable 2>/dev/null | head -1)
    if [ -n "$TOR_BIN" ]; then
        cp "$TOR_BIN" "$OUT_DIR/usr/bin/tor"
        chmod 755 "$OUT_DIR/usr/bin/tor"
        mkdir -p /tmp/tor-libs64
        curl -s --max-time 20 -L -o /tmp/musl64.apk "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/musl-1.2.5-r3.apk" 2>> $LOG
        tar -xzf /tmp/musl64.apk -C /tmp/tor-libs64 2>/dev/null
        cp /tmp/tor-libs64/lib/ld-musl-x86_64.so.1 "$OUT_DIR/lib/" 2>/dev/null
        for pkg in libxtables libip4tc libip6tc libssl3 libcrypto3 libevent zlib liblzma libzstd libseccomp libcap; do
            APK=$(curl -s --max-time 10 "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/" 2>/dev/null | grep -oE "${pkg}-[0-9][^\"]*\.apk" | head -1)
            if [ -n "$APK" ]; then
                curl -s --max-time 20 -L -o /tmp/${pkg}64.apk "https://dl-cdn.alpinelinux.org/alpine/v3.20/main/x86_64/$APK" 2>> $LOG
                tar -xzf /tmp/${pkg}64.apk -C /tmp/tor-libs64 2>/dev/null
            fi
        done
        find /tmp/tor-libs64 -name "*.so*" -exec cp -a {} "$OUT_DIR/lib/" \; 2>/dev/null
        cd "$OUT_DIR/lib"
        for f in *.so.*.*; do
            base=$(echo "$f" | sed 's/\.[0-9]*\.[0-9]*$//')
            [ ! -e "$base" ] && ln -sf "$f" "$base" 2>/dev/null
        done
        cd /
        ln -sf ld-musl-x86_64.so.1 "$OUT_DIR/lib/libc.musl-x86_64.so.1" 2>/dev/null
        echo "tor + libs installed" >> $LOG
    fi
fi

cat > "$OUT_DIR/etc/tor/torrc" << 'TORRC'
DataDirectory /var/lib/tor
Log notice stderr
SocksPort 9050
ExitPolicy reject *:*
User root
TORRC
chmod 700 "$OUT_DIR/var/lib/tor"

# Pack
cd "$OUT_DIR"
find . | cpio --create --format=newc --owner=root:root | gzip > "$HOME/wltios-build/wltios-initramfs64.cpio.gz"
ls -lh "$HOME/wltios-build/wltios-initramfs64.cpio.gz" >> $LOG

echo "=== Contents ===" >> $LOG
ls -la "$OUT_DIR/sbin/" >> $LOG
ls -la "$OUT_DIR/bin/" >> $LOG
ls -la "$OUT_DIR/usr/bin/" >> $LOG
echo "lib count: $(ls "$OUT_DIR/lib/" | wc -l)" >> $LOG

echo [[BUILD64 DONE]] >> $LOG
date >> $LOG