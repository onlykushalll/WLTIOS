#!/bin/bash
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-binary-wltshell

echo "=== Compile wlt-shell binary ==="
cat > /tmp/wlt_shell_bin.c << 'WLTBC'
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

/* Compiled wlt-shell replacement: prints ps listing to inherited stdout.
 * The wlt-init applet (PID 1) execve's this binary. */
int main(void) {
    /* Mount filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);

    /* Print process listing */
    printf("=====WLTIOS-PS-BEGIN=====\n");
    fflush(stdout);

    /* Run ps via fork+exec */
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {"/bin/ps", "aux", NULL};
        char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", NULL};
        execve("/bin/ps", argv, envp);
        /* If ps aux fails, try plain ps */
        char *argv2[] = {"/bin/ps", NULL};
        execve("/bin/ps", argv2, envp);
        _exit(127);
    }
    if (pid > 0) waitpid(pid, NULL, 0);

    printf("=====WLTIOS-PS-END=====\n");
    fflush(stdout);

    /* Check for tor */
    FILE *fp = popen("ps 2>/dev/null | grep -c '[t]or'", "r");
    if (fp) {
        int count = 0;
        fscanf(fp, "%d", &count);
        pclose(fp);
        printf("[wlt-check] tor present: %d\n", count);
        fflush(stdout);
    }

    /* Print tor log if exists */
    fp = fopen("/tmp/tor.log", "r");
    if (fp) {
        printf("=====WLTIOS-TOR-LOG=====\n");
        fflush(stdout);
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        fclose(fp);
        printf("=====WLTIOS-TOR-LOG-END=====\n");
        fflush(stdout);
    }

    printf("[wlt-shell] Dropping to /bin/sh\n");
    fflush(stdout);

    /* Drop to shell */
    char *argv[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", argv, envp);
    for (;;) sleep(1);
    return 0;
}
WLTBC
gcc -static -o /tmp/wlt_shell_bin /tmp/wlt_shell_bin.c 2>&1
file /tmp/wlt_shell_bin
echo "Compiled OK"

echo "=== Extract base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Replace /bin/wlt-shell with compiled binary
cp /tmp/wlt_shell_bin bin/wlt-shell
chmod +x bin/wlt-shell
echo "wlt-shell: $(file bin/wlt-shell)"

# vm0
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
for lib in libz.so.1 libm.so.6 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.so.6 ld-linux-x86-64.so.2; do
    for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        mkdir -p "$(dirname "$rel")"
        cp -P "$f" "$rel" 2>/dev/null || true
    done
done
cat > etc/init.d/S99tor << 'TORINIT'
#!/bin/sh
case "$1" in
  start)
    echo "[tor-pd] Starting Tor daemon in vm1..."
    mkdir -p /var/lib/tor /var/run /tmp
    /usr/bin/tor -f /etc/tor/torrc > /tmp/tor.log 2>&1 &
    echo $! > /var/run/tor.pid
    echo "[tor-pd] Tor started (pid=$(cat /var/run/tor.pid 2>/dev/null))"
    ;;
esac
exit 0
TORINIT
chmod +x etc/init.d/S99tor
find . -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c1-binary-wltshell.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== RESULTS ==="
echo "--- PS listing ---"
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -25
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor present\|tor-pd\|Starting Tor\|Dropping" | head -10
echo "--- tor log ---"
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
echo "--- execve errors ---"
strings "$LOG" | grep "execve\|Failed to execute" | head -3
echo "--- last 10 ---"
strings "$LOG" | tail -10
