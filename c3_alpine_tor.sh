#!/bin/bash
# C.3-fix v3: Use Alpine Tor (musl-based, no glibc ISA level check).
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
ALPINE_DIR=/home/kushal/wltios-build/tor-libs-alpine
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-alpine-tor

echo "=== Check Alpine Tor ==="
file "$ALPINE_DIR/tor" 2>/dev/null
readelf -d "$ALPINE_DIR/tor" 2>/dev/null | grep NEEDED | head -15
readelf -l "$ALPINE_DIR/tor" 2>/dev/null | grep interpreter

echo ""
echo "=== Check Alpine Tor deps ==="
ls -la "$ALPINE_DIR/" 2>/dev/null | head -20

echo ""
echo "=== Compile wlt-shell v3 ==="
cat > /tmp/wlt_shell_v3.c << 'WLTBC3'
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
static int file_exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
int main(void) {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    usleep(100000);
    if (file_exists("/usr/bin/tor")) {
        printf("[tor-pd] Starting Tor daemon...\n");
        fflush(stdout);
        mkdir("/var/lib/tor", 0755);
        mkdir("/var/run", 0755);
        mkdir("/tmp", 0755);
        pid_t tor_pid = fork();
        if (tor_pid == 0) {
            char *argv[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            int logfd = open("/tmp/tor.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (logfd >= 0) { dup2(logfd, 1); dup2(logfd, 2); close(logfd); }
            execve("/usr/bin/tor", argv, envp);
            _exit(127);
        }
        printf("[tor-pd] Tor started (pid=%d)\n", tor_pid);
        fflush(stdout);
        sleep(8);
    }
    printf("=====WLTIOS-PS-BEGIN=====\n");
    fflush(stdout);
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            int is_pid = 1;
            for (int i = 0; e->d_name[i]; i++) {
                if (e->d_name[i] < '0' || e->d_name[i] > '9') { is_pid = 0; break; }
            }
            if (!is_pid) continue;
            char comm_path[512];
            snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", e->d_name);
            FILE *fp = fopen(comm_path, "r");
            if (fp) {
                char comm[256];
                if (fgets(comm, sizeof(comm), fp)) {
                    int len = strlen(comm);
                    if (len > 0 && comm[len-1] == '\n') comm[len-1] = '\0';
                    printf("PID %s: %s\n", e->d_name, comm);
                }
                fclose(fp);
            }
        }
        closedir(d);
    }
    printf("=====WLTIOS-PS-END=====\n");
    fflush(stdout);
    int tor_found = 0;
    d = opendir("/proc");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            int is_pid = 1;
            for (int i = 0; e->d_name[i]; i++) {
                if (e->d_name[i] < '0' || e->d_name[i] > '9') { is_pid = 0; break; }
            }
            if (!is_pid) continue;
            char comm_path[512];
            snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", e->d_name);
            FILE *fp = fopen(comm_path, "r");
            if (fp) {
                char comm[256];
                if (fgets(comm, sizeof(comm), fp)) {
                    if (strstr(comm, "tor")) tor_found++;
                }
                fclose(fp);
            }
        }
        closedir(d);
    }
    printf("[wlt-check] tor processes found: %d\n", tor_found);
    fflush(stdout);
    FILE *fp = fopen("/tmp/tor.log", "r");
    if (fp) {
        printf("=====WLTIOS-TOR-LOG=====\n");
        fflush(stdout);
        char buf[512];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        fclose(fp);
        printf("=====WLTIOS-TOR-LOG-END=====\n");
        fflush(stdout);
    }
    printf("[wlt-shell] Done.\n");
    fflush(stdout);
    char *argv[] = {"/bin/sh", NULL};
    char *envp[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", argv, envp);
    for (;;) sleep(1);
    return 0;
}
WLTBC3
gcc -static -o /tmp/wlt_shell_v3 /tmp/wlt_shell_v3.c 2>&1
echo "Compiled: $(file /tmp/wlt_shell_v3 | grep -o 'ELF 64-bit')"

echo "=== Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

# Copy Alpine Tor + ALL its libs
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib
cp "$ALPINE_DIR/tor" usr/bin/tor
chmod +x usr/bin/tor

# Copy ALL libs from Alpine dir
if [ -d "$ALPINE_DIR/lib" ]; then
    cp -a "$ALPINE_DIR/lib/"* lib/ 2>/dev/null || true
fi
if [ -d "$ALPINE_DIR/lib64" ]; then
    mkdir -p lib64
    cp -a "$ALPINE_DIR/lib64/"* lib64/ 2>/dev/null || true
fi

# Also copy torrc from rootfs64 (Alpine may not have it)
cp /home/kushal/wltios-build/wltios-rootfs64/etc/tor/torrc etc/tor/torrc 2>/dev/null || true
cp -r /home/kushal/wltios-build/wltios-rootfs64/usr/share/tor/* usr/share/tor/ 2>/dev/null || true

# Fix torrc: Alpine Tor may need different paths
cat > etc/tor/torrc << 'TORRC'
DataDirectory /var/lib/tor
Log notice file /tmp/tor.log
SocksPort 9050
ExitPolicy reject *:*
User root
TORRC

echo "=== Verify Alpine Tor deps ==="
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | head -15
echo "---interpreter---"
readelf -l usr/bin/tor 2>/dev/null | grep interpreter
echo "---deps check---"
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find lib lib64 -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild seL4 image ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo "=== Boot test ==="
LOG=/home/kushal/torpd-c3-alpine-tor.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== Results ==="
echo "--- tor-pd ---"
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo "--- tor PIDs ---"
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo "--- wlt-check ---"
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo "--- tor log ---"
strings "$LOG" | grep -A25 "WLTIOS-TOR-LOG" | head -30
echo "--- CPU ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
