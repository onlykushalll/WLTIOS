#!/bin/bash
# C.3-fix: Use WSL system glibc (compiled for THIS CPU) to fix ISA level error.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-sysglibc-final

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

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v3 bin/wlt-shell
chmod +x bin/wlt-shell

mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true

# Tor-specific libs from wltios-rootfs64 (NOT glibc)
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 libsystemd.so.0 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2; do
    for f in $(find "$TOR_SRC/lib" "$TOR_SRC/lib64" -name "$lib*" 2>/dev/null); do
        rel="${f#$TOR_SRC/}"
        mkdir -p "$(dirname "$rel")"
        cp -P "$f" "$rel" 2>/dev/null || true
    done
done

# WSL SYSTEM glibc (compiled for THIS CPU — no ISA mismatch)
cp /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 lib64/ld-linux-x86-64.so.2
cp /lib/x86_64-linux-gnu/libc.so.6 lib/libc.so.6
cp /lib/x86_64-linux-gnu/libm.so.6 lib/libm.so.6
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

echo "=== Tor deps check ==="
LD_LIBRARY_PATH="$WORK/lib:$WORK/lib64" "$WORK/lib64/ld-linux-x86-64.so.2" --list "$WORK/usr/bin/tor" 2>&1 | head -20

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
echo "BUILD DONE"
