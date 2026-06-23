#!/bin/bash
set -e
WLTIOS_SRC="/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src"
OUT_DIR="$HOME/wltios-build/wltios-rootfs"
BUILD_DIR="$HOME/wltios-build/wltios-compile"

rm -rf "$OUT_DIR" "$BUILD_DIR"
mkdir -p "$OUT_DIR"/{bin,sbin,lib,proc,sys,dev,tmp,var/log,etc,run,root,home}
mkdir -p "$BUILD_DIR"

CFLAGS="-m32 -static -Wall -std=gnu11 -O2"
COMMON_INC="-include stdint.h -include string.h -include stdlib.h -include stdio.h -include unistd.h -include errno.h -D_GNU_SOURCE"

echo "=== Building WLTIOS Initramfs (v2) ==="

echo "--- 1. Compile init-minimal ---"
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
gcc $CFLAGS $COMMON_INC \
    $SHELL_SRCS \
    -I"${SHELL_DIR}/include" \
    -o "$BUILD_DIR/wlt-shell" 2>&1 || {
    echo "  [WARN] shell failed â writing simple shell wrapper"
    # Write a simple shell that reads commands and execs them
    cat > /tmp/simple-shell.c << 'SH'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
int main(void) {
    char buf[1024];
    printf("WLTIOS Shell v1.0\nType 'help' for commands.\n");
    fflush(stdout);
    for(;;) {
        printf("/wlt# ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = 0;
        if (buf[0] == 0) continue;
        if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) break;
        if (strcmp(buf, "help") == 0) {
            printf("Commands: ls, cat, echo, pwd, cd, ps, kill, mount, umount, exit, help\n");
            fflush(stdout);
            continue;
        }
        pid_t pid = fork();
        if (pid == 0) {
            execl("/bin/sh", "sh", "-c", buf, NULL);
            _exit(127);
        }
        waitpid(pid, NULL, 0);
    }
    return 0;
}
SH
    gcc $CFLAGS $COMMON_INC /tmp/simple-shell.c -o "$BUILD_DIR/wlt-shell" 2>&1
}
file "$BUILD_DIR/wlt-shell"
echo ""

echo "--- 3. Install busybox (64-bit, may not work in 32-bit QEMU) ---"
if [ -f /bin/busybox ]; then
    cp /bin/busybox "$OUT_DIR/bin/busybox"
    for cmd in sh ash ls cat echo mkdir cp mv rm mount umount ps kill grep sleep; do
        ln -sf busybox "$OUT_DIR/bin/$cmd" 2>/dev/null || true
    done
    echo "  busybox installed"
fi

echo "--- 4. Install binaries ---"
cp "$BUILD_DIR/wlt-init-minimal" "$OUT_DIR/sbin/init" && echo "  /sbin/init OK"
cp "$BUILD_DIR/wlt-shell" "$OUT_DIR/bin/wlt-shell" && echo "  /bin/wlt-shell OK"

echo "--- 5. Create /etc ---"
echo "WLTIOS_VERSION=0.1.0" > "$OUT_DIR/etc/wltios-release"
echo "wltios" > "$OUT_DIR/etc/hostname"

echo "--- 6. Create /dev ---"
mknod "$OUT_DIR/dev/console" c 5 1 2>/dev/null || true
mknod "$OUT_DIR/dev/null" c 1 3 2>/dev/null || true
mknod "$OUT_DIR/dev/ttyS0" c 4 64 2>/dev/null || true

echo "--- 7. Pack initramfs ---"
cd "$OUT_DIR"
find . | cpio --create --format=newc | gzip > "$HOME/wltios-build/wltios-initramfs.cpio.gz"
echo ""
echo "=== BUILD COMPLETE ==="
ls -lh "$HOME/wltios-build/wltios-initramfs.cpio.gz"
echo ""
echo "=== Contents ==="
zcat "$HOME/wltios-build/wltios-initramfs.cpio.gz" | cpio -t 2>/dev/null | head -20
