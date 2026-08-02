#!/bin/bash
# C.1-fix: Build rootfs with a COMPILED /init binary (not a shell script).
# The shell script approach failed because busybox intercepts PID 1 exec
# and runs its init applet instead of interpreting the script.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-binary-init

echo "=== Compile wlt_init binary (64-bit static) ==="
cat > /tmp/wlt_init2.c << 'INITC'
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>

/* WLTIOS guest init: mounts filesystems, runs /etc/init.d/S* scripts,
 * prints process listing, drops to /bin/sh.
 * This is a COMPILED binary (not a shell script) because busybox
 * intercepts PID 1 exec and runs its init applet instead of
 * interpreting a shell script. */

static void run_script(const char *path) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char*)path, "start", NULL};
        execv(path, argv);
        _exit(127);
    }
}

int main(void) {
    /* Redirect stdio to /dev/console */
    int console = open("/dev/console", O_RDWR);
    if (console >= 0) {
        dup2(console, 0);
        dup2(console, 1);
        dup2(console, 2);
        if (console > 2) close(console);
    }

    /* Mount essential filesystems */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);

    /* Print boot marker */
    printf("=====WLTIOS-INIT-RAN=====\n");
    fflush(stdout);

    /* Run all /etc/init.d/S* scripts in background */
    DIR *d = opendir("/etc/init.d");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == 'S') {
                char path[256];
                snprintf(path, sizeof(path), "/etc/init.d/%s", e->d_name);
                run_script(path);
            }
        }
        closedir(d);
    }

    /* Wait for init scripts (tor start, ps-check) */
    sleep(8);

    /* Print process listing */
    printf("=====WLTIOS-PS-BEGIN=====\n");
    fflush(stdout);
    /* Run ps */
    pid_t ps_pid = fork();
    if (ps_pid == 0) {
        char *argv[] = {"/bin/ps", "aux", NULL};
        char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", NULL};
        execve("/bin/ps", argv, envp);
        /* If ps aux fails, try plain ps */
        char *argv2[] = {"/bin/ps", NULL};
        execve("/bin/ps", argv2, envp);
        _exit(127);
    }
    waitpid(ps_pid, NULL, 0);
    printf("=====WLTIOS-PS-END=====\n");
    fflush(stdout);

    /* Check if tor is running */
    int tor_count = 0;
    FILE *fp = popen("ps 2>/dev/null | grep -c '[t]or'", "r");
    if (fp) {
        fscanf(fp, "%d", &tor_count);
        pclose(fp);
    }
    printf("[wlt-check] tor present: %d\n", tor_count);
    fflush(stdout);

    /* Print tor log if it exists */
    fp = fopen("/tmp/tor.log", "r");
    if (fp) {
        printf("[wlt-check] tor log:\n");
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            printf("%s", buf);
        }
        fclose(fp);
    }
    printf("=====WLTIOS-INIT-DONE=====\n");
    fflush(stdout);

    /* Drop to shell */
    char *argv[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", argv, envp);

    /* If exec fails, loop */
    for (;;) sleep(1);
    return 0;
}
INITC

gcc -static -o /tmp/wlt_init2 /tmp/wlt_init2.c 2>&1
file /tmp/wlt_init2
echo "Compiled OK"

echo ""
echo "=== Extract base rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Replace /init with compiled binary
cp /tmp/wlt_init2 init
chmod +x init
echo "New /init: $(file init)"

# Remove /sbin/init symlink (so busybox doesn't intercept)
rm -f sbin/init
# Also remove wlt-shell
rm -f bin/wlt-shell

# --- vm0 (no Tor) ---
echo "=== Building vm0 ==="
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# --- vm1 (has Tor) ---
echo "=== Building vm1 ==="
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

# Add S99tor init script for vm1
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

echo ""
echo "=== Verify ==="
echo "vm0 /init: $(cd /tmp && rm -rf v0 && mkdir v0 && cd v0 && cpio -idm < "$VM0_OUT" 2>/dev/null && file init)"
echo "vm0 sbin/init: $(cd /tmp/v0 && ls sbin/init 2>&1)"
echo "vm1 /init: $(cd /tmp && rm -rf v1 && mkdir v1 && cd v1 && cpio -idm < "$VM1_OUT" 2>/dev/null && file init)"
echo "vm1 has tor: $(cd /tmp/v1 && file usr/bin/tor 2>/dev/null)"
echo "vm0 no tor: $(cd /tmp/v0 && ls usr/bin/tor 2>&1)"
echo "[DONE]"
