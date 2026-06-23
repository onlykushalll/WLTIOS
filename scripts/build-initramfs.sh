#!/bin/bash
set -e
WLTIOS_SRC="/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src"
OUT_DIR="$HOME/wltios-build/wltios-rootfs"
BUILD_DIR="$HOME/wltios-build/wltios-compile"

rm -rf "$OUT_DIR" "$BUILD_DIR"
mkdir -p "$OUT_DIR"/{bin,sbin,lib,proc,sys,dev,tmp,var/log,etc,run,root,home}
mkdir -p "$BUILD_DIR"

CFLAGS="-m32 -static -Wall -Wextra -Werror -std=gnu11 -O0 -fno-merge-constants -fno-merge-all-constants"
COMMON_INC="-include stdint.h -include string.h -include stdlib.h -include stdio.h -include unistd.h -include errno.h -include fcntl.h -D_GNU_SOURCE"

echo "=== Building WLTIOS Initramfs (real modules) ==="

# 1. init-minimal
gcc $CFLAGS $COMMON_INC -include sys/wait.h -include sys/mount.h -include sys/reboot.h -include linux/reboot.h \
    "${WLTIOS_SRC}/userspace/wlt-init/init-minimal.c" -o "$BUILD_DIR/wlt-init-minimal" 2>&1
echo "  init OK"

# 2. wlt-shell (cmds/shell.c — real CLI shell)
gcc $CFLAGS $COMMON_INC "${WLTIOS_SRC}/userspace/wlt-shell/cmds/shell.c" \
    -I"${WLTIOS_SRC}/userspace/wlt-shell/include" -o "$BUILD_DIR/wlt-shell" 2>&1
echo "  shell OK"

# 3. wlt-net (REAL source — ALL .c files with -DWLT_TESTING to exclude CLI main)
cat > /tmp/wlt-net-svc.c << 'NETEOF'
#include "net.h"
#include <string.h>
int main(void) {
    wlt_net_state_t state;
    memset(&state, 0, sizeof(state));
    wlt_net_error_t err = wlt_net_config_defaults(&state);
    if (err != WLT_NET_OK) return 1;
    write(1, "[wlt-net] Network configured (Tor-only mode)\r\n", 45);
    wlt_net_shutdown(&state);
    return 0;
}
NETEOF
NET_DIR="${WLTIOS_SRC}/userspace/wlt-net"
NET_SRCS=$(find "$NET_DIR" -maxdepth 1 -name "*.c" ! -name "*_test*" 2>/dev/null | tr '\n' ' ')
gcc $CFLAGS $COMMON_INC -DWLT_TESTING /tmp/wlt-net-svc.c $NET_SRCS -I"$NET_DIR/include" -o "$BUILD_DIR/wlt-net" 2>&1
echo "  wlt-net OK (real source)"

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

# 5. Install
cp "$BUILD_DIR/wlt-init-minimal" "$OUT_DIR/sbin/init"
cp "$BUILD_DIR/wlt-shell" "$OUT_DIR/bin/wlt-shell"
cp "$BUILD_DIR/wlt-net" "$OUT_DIR/sbin/wlt-net"
cp "$BUILD_DIR/wlt-crypto" "$OUT_DIR/sbin/wlt-crypto"
echo '#!/bin/sh' > "$OUT_DIR/init"; echo 'exec /sbin/init' >> "$OUT_DIR/init"; chmod 755 "$OUT_DIR/init"
echo "WLTIOS_VERSION=0.1.0" > "$OUT_DIR/etc/wltios-release"
echo "wltios" > "$OUT_DIR/etc/hostname"
mknod "$OUT_DIR/dev/console" c 5 1 2>/dev/null || true
mknod "$OUT_DIR/dev/null" c 1 3 2>/dev/null || true
mknod "$OUT_DIR/dev/ttyS0" c 4 64 2>/dev/null || true

# 6. Pack
cd "$OUT_DIR"
find . | cpio --create --format=newc | gzip > "$HOME/wltios-build/wltios-initramfs.cpio.gz"
ls -lh "$HOME/wltios-build/wltios-initramfs.cpio.gz"
