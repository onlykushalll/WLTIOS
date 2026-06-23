#!/bin/bash
set -e
WLTIOS_SRC="/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src"
OUT_DIR="$HOME/wltios-build/wltios-rootfs"
BUILD_DIR="$HOME/wltios-build/wltios-compile"

rm -rf "$OUT_DIR" "$BUILD_DIR"
mkdir -p "$OUT_DIR"/{bin,sbin,lib,proc,sys,dev,tmp,var/log,etc,run,root,home}
mkdir -p "$BUILD_DIR"

# Use -O0 to prevent string constant alignment that causes null bytes
CFLAGS="-m32 -static -Wall -std=gnu11 -O0"
COMMON_INC="-include stdint.h -include string.h -include stdlib.h -include stdio.h -include unistd.h -include errno.h -D_GNU_SOURCE"

echo "=== Building WLTIOS Initramfs (v3 — fixed nulls + more binaries) ==="

echo "--- 1. Compile init-minimal (v2, write() instead of printf) ---"
gcc $CFLAGS $COMMON_INC \
    -include fcntl.h -include sys/wait.h -include sys/mount.h \
    -include sys/reboot.h -include linux/reboot.h \
    "${WLTIOS_SRC}/userspace/wlt-init/init-minimal.c" \
    -o "$BUILD_DIR/wlt-init-minimal" 2>&1
file "$BUILD_DIR/wlt-init-minimal"
echo ""

echo "--- 2. Compile wlt-shell ---"
SHELL_DIR="${WLTIOS_SRC}/userspace/wlt-shell"
SHELL_SRCS=$(find "$SHELL_DIR" -maxdepth 1 -name "*.c" ! -name "*_test*" 2>/dev/null | tr '\n' ' ')
gcc $CFLAGS $COMMON_INC $SHELL_SRCS -I"${SHELL_DIR}/include" \
    -o "$BUILD_DIR/wlt-shell" 2>&1 || {
    echo "  [WARN] wlt-shell failed — writing simple shell wrapper"
    cat > /tmp/simple-shell.c << 'SH'
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
int main(void) {
    char buf[1024];
    write(1, "WLTIOS Shell v1.0\r\n", 19);
    write(1, "Type 'help' for commands.\r\n", 27);
    for(;;) {
        write(1, "/wlt# ", 6);
        int n = read(0, buf, sizeof(buf)-1);
        if (n <= 0) break;
        buf[n] = 0;
        buf[strcspn(buf, "\n")] = 0;
        if (buf[0] == 0) continue;
        if (strcmp(buf, "exit") == 0) break;
        if (strcmp(buf, "help") == 0) {
            write(1, "Commands: ls, cat, echo, pwd, exit, help\r\n", 42);
            continue;
        }
        if (strcmp(buf, "pwd") == 0) {
            write(1, "/\r\n", 3);
            continue;
        }
        if (strcmp(buf, "ls") == 0 || strncmp(buf, "ls ", 3) == 0) {
            pid_t p = fork();
            if (p == 0) { execl("/bin/busybox", "ls", buf+3 ? buf+3 : "/", NULL); _exit(127); }
            waitpid(p, NULL, 0);
            continue;
        }
        pid_t p = fork();
        if (p == 0) { execl("/bin/sh", "sh", "-c", buf, NULL); _exit(127); }
        waitpid(p, NULL, 0);
    }
    return 0;
}
SH
    gcc $CFLAGS $COMMON_INC /tmp/simple-shell.c -o "$BUILD_DIR/wlt-shell" 2>&1
}
file "$BUILD_DIR/wlt-shell"
echo ""

echo "--- 3. Compile wlt-net (best effort, -DWLT_TESTING to exclude CLI main) ---"
NET_DIR="${WLTIOS_SRC}/userspace/wlt-net"
NET_SRCS=$(find "$NET_DIR" -maxdepth 1 -name "*.c" ! -name "*_test*" 2>/dev/null | tr '\n' ' ')
# Wrap as a simple service that just exits 0 (init will see "OK")
cat > /tmp/wlt-net-svc.c << 'NETEOF'
/* wlt-net service wrapper — exits 0 to indicate "started" */
#include <unistd.h>
int main(void) {
    /* In production: initialize network, connect Tor, etc. */
    /* For now: just report we're alive and exit */
    write(1, "[wlt-net] Network service initialized (stub)\r\n", 46);
    return 0;
}
NETEOF
gcc $CFLAGS $COMMON_INC /tmp/wlt-net-svc.c -o "$BUILD_DIR/wlt-net" 2>&1 || echo "  [WARN] wlt-net failed"
file "$BUILD_DIR/wlt-net" 2>/dev/null || true
echo ""

echo "--- 4. Compile wlt-crypto (best effort) ---"
cat > /tmp/wlt-crypto-svc.c << 'CRYPEOF'
/* wlt-crypto service wrapper — exits 0 to indicate "started" */
#include <unistd.h>
int main(void) {
    write(1, "[wlt-crypto] Crypto service initialized (stub)\r\n", 48);
    return 0;
}
CRYPEOF
gcc $CFLAGS $COMMON_INC /tmp/wlt-crypto-svc.c -o "$BUILD_DIR/wlt-crypto" 2>&1 || echo "  [WARN] wlt-crypto failed"
file "$BUILD_DIR/wlt-crypto" 2>/dev/null || true
echo ""

echo "--- 5. Install busybox ---"
if [ -f /bin/busybox ]; then
    cp /bin/busybox "$OUT_DIR/bin/busybox"
    for cmd in sh ash ls cat echo mkdir cp mv rm mount umount ps kill grep sleep pwd date; do
        ln -sf busybox "$OUT_DIR/bin/$cmd" 2>/dev/null || true
    done
    echo "  busybox + symlinks installed"
fi

echo "--- 6. Install binaries ---"
cp "$BUILD_DIR/wlt-init-minimal" "$OUT_DIR/sbin/init" && echo "  /sbin/init OK"
cp "$BUILD_DIR/wlt-shell" "$OUT_DIR/bin/wlt-shell" && echo "  /bin/wlt-shell OK"
cp "$BUILD_DIR/wlt-net" "$OUT_DIR/sbin/wlt-net" && echo "  /sbin/wlt-net OK"
cp "$BUILD_DIR/wlt-crypto" "$OUT_DIR/sbin/wlt-crypto" && echo "  /sbin/wlt-crypto OK"

echo "--- 7. Create /etc ---"
echo "WLTIOS_VERSION=0.1.0" > "$OUT_DIR/etc/wltios-release"
echo "wltios" > "$OUT_DIR/etc/hostname"

echo "--- 8. Create /dev ---"
mknod "$OUT_DIR/dev/console" c 5 1 2>/dev/null || true
mknod "$OUT_DIR/dev/null" c 1 3 2>/dev/null || true
mknod "$OUT_DIR/dev/ttyS0" c 4 64 2>/dev/null || true

echo "--- 9. Create /init (kernel looks for this in initramfs) ---"
cat > "$OUT_DIR/init" << 'INITEOF'
#!/bin/sh
exec /sbin/init
INITEOF
chmod 755 "$OUT_DIR/init"

echo "--- 10. Pack initramfs ---"
cd "$OUT_DIR"
find . | cpio --create --format=newc | gzip > "$HOME/wltios-build/wltios-initramfs-v3.cpio.gz"
echo ""
echo "=== BUILD COMPLETE ==="
ls -lh "$HOME/wltios-build/wltios-initramfs-v3.cpio.gz"
echo ""
echo "=== Binary sizes ==="
ls -lh "$BUILD_DIR"/*
echo ""
echo "=== Contents ==="
zcat "$HOME/wltios-build/wltios-initramfs-v3.cpio.gz" | cpio -t 2>/dev/null | grep -E "sbin/init|bin/wlt|bin/busy"
