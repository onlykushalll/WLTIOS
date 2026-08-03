#!/bin/bash
# C.3-final: Patched ld-linux + error reporting in wlt-shell.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-final-diag

echo "=== Patch ld-linux ==="
python3 << 'PYEOF'
ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"
with open(ld_path, "rb") as f:
    data = bytearray(f.read())
error_str = b"CPU ISA level is lower than required"
str_offset = data.find(error_str)
str_offset2 = data.find(b"is lower than required")
for i in range(len(data) - 7):
    if data[i] == 0x48 and data[i+1] == 0x8d:
        for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
            if data[i+2] == reg:
                rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                target = i + 7 + rel
                if target == str_offset or target == str_offset2:
                    for j in range(i - 32, i - 2):
                        if j < 0: continue
                        b = data[j]
                        if b == 0x0F and j+1 < len(data) and 0x80 <= data[j+1] <= 0x8F:
                            for k in range(6): data[j+k] = 0x90
                            break
                        elif 0x70 <= b <= 0x7F:
                            data[j] = 0x90; data[j+1] = 0x90
                            break
with open("/tmp/ld-linux-patched.so.2", "wb") as f:
    f.write(data)
print("Patched ld-linux OK")
PYEOF

echo "=== Compile wlt-shell v5 (with execve error reporting) ==="
cat > /tmp/wlt_shell_v5.c << 'WLTBC5'
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
int main(void) {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    usleep(100000);
    if (file_exists("/usr/bin/tor")) {
        printf("[tor-pd] Starting Tor daemon...\n"); fflush(stdout);
        mkdir("/var/lib/tor", 0755); mkdir("/var/run", 0755); mkdir("/tmp", 0755);
        pid_t tp = fork();
        if (tp == 0) {
            /* Child: open log, write exec status, exec Tor */
            int lf = open("/tmp/tor.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if (lf >= 0) {
                dup2(lf, 1); dup2(lf, 2); close(lf);
            }
            printf("TOR_EXEC_ATTEMPT pid=%d\n", getpid());
            printf("TOR_EXEC: about to execve /usr/bin/tor\n");
            fflush(stdout);
            char *av[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            execve("/usr/bin/tor", av, ev);
            /* If we get here, execve failed */
            printf("TOR_EXEC_FAILED errno=%d (%s)\n", errno, strerror(errno));
            fflush(stdout);
            _exit(127);
        }
        printf("[tor-pd] Tor started (pid=%d)\n", tp); fflush(stdout);
        sleep(15);
    }
    printf("=====WLTIOS-PS-BEGIN=====\n"); fflush(stdout);
    DIR *d = opendir("/proc");
    if (d) { struct dirent *e;
        while ((e = readdir(d))) {
            int ip = 1;
            for (int i = 0; e->d_name[i]; i++) if (e->d_name[i]<'0'||e->d_name[i]>'9') { ip=0; break; }
            if (!ip) continue;
            char cp[512]; snprintf(cp, sizeof(cp), "/proc/%s/comm", e->d_name);
            FILE *fp = fopen(cp, "r");
            if (fp) { char c[256]; if (fgets(c, sizeof(c), fp)) { int l=strlen(c); if(l>0&&c[l-1]=='\n')c[l-1]=0; printf("PID %s: %s\n", e->d_name, c); } fclose(fp); }
        }
        closedir(d);
    }
    printf("=====WLTIOS-PS-END=====\n"); fflush(stdout);
    int tf = 0;
    d = opendir("/proc");
    if (d) { struct dirent *e;
        while ((e = readdir(d))) {
            int ip = 1;
            for (int i = 0; e->d_name[i]; i++) if (e->d_name[i]<'0'||e->d_name[i]>'9') { ip=0; break; }
            if (!ip) continue;
            char cp[512]; snprintf(cp, sizeof(cp), "/proc/%s/comm", e->d_name);
            FILE *fp = fopen(cp, "r");
            if (fp) { char c[256]; if (fgets(c, sizeof(c), fp)) if (strstr(c, "tor")) tf++; fclose(fp); }
        }
        closedir(d);
    }
    printf("[wlt-check] tor processes found: %d\n", tf); fflush(stdout);
    FILE *fp = fopen("/tmp/tor.log", "r");
    if (fp) { printf("=====WLTIOS-TOR-LOG=====\n"); fflush(stdout); char b[512]; while(fgets(b,sizeof(b),fp))printf("%s",b); fclose(fp); printf("=====WLTIOS-TOR-LOG-END=====\n"); fflush(stdout); }
    printf("[wlt-shell] Done.\n"); fflush(stdout);
    char *av[] = {"/bin/sh", NULL};
    char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", av, ev);
    for(;;) sleep(1);
    return 0;
}
WLTBC5
gcc -static -o /tmp/wlt_shell_v5 /tmp/wlt_shell_v5.c 2>&1
echo "Compiled: $(file /tmp/wlt_shell_v5 | grep -o 'ELF 64-bit')"

echo "=== Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v5 bin/wlt-shell
chmod +x bin/wlt-shell

mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
cp /tmp/ld-linux-patched.so.2 lib64/ld-linux-x86-64.so.2
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v5 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo "=== Boot ==="
LOG=/home/kushal/torpd-c3-final-diag.log
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
strings "$LOG" | grep -A30 "WLTIOS-TOR-LOG" | head -35
echo "--- CPU ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
