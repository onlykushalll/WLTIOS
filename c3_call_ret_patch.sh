#!/bin/bash
# C.3-FINAL: Patch ld-linux by replacing call _dl_signal_error with ret.
# The call is at offset 0xf304 (5 bytes: e8 37 2f ff ff).
# Replace with: ret (c3) + 4 NOPs.
set -e

echo "=== Patch ld-linux: replace call _dl_signal_error with ret ==="
python3 << 'PYEOF'
ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"
with open(ld_path, "rb") as f:
    data = bytearray(f.read())

# The call instruction at 0xf304: e8 37 2f ff ff
# Verify it's there
assert data[0xf304] == 0xe8, f"Expected 0xe8 (call) at 0xf304, got 0x{data[0xf304]:02x}"
print(f"Verified: call at 0xf304 (bytes: {' '.join(f'{data[0xf304+i]:02x}' for i in range(5))})")

# Replace with: ret (c3) + 4 NOPs (90)
data[0xf304] = 0xc3  # ret
data[0xf305] = 0x90  # nop
data[0xf306] = 0x90  # nop
data[0xf307] = 0x90  # nop
data[0xf308] = 0x90  # nop
print("Patched: call _dl_signal_error -> ret + 4 NOPs")

# Also check for the second reference at 0x27eb2
# Let me find what's at 0x27eb2
print(f"\nBytes at 0x27eb2: {' '.join(f'{data[0x27eb2+i]:02x}' for i in range(16))}")
# Check if there's a call near 0x27eb2
for i in range(0x27eb2, 0x27ec2):
    if data[i] == 0xe8:
        print(f"Found call at 0x{i:x}")
        # Replace with ret + NOPs
        data[i] = 0xc3
        for j in range(i+1, i+5):
            data[j] = 0x90
        print(f"Patched call at 0x{i:x} -> ret + NOPs")

# Also search for any other calls to _dl_signal_error that reference the ISA string
# by looking for the pattern: lea ... -> call _dl_signal_error
error_str = b"CPU ISA level is lower than required"
str_offset = data.find(error_str)
for i in range(len(data) - 7):
    if data[i] == 0x48 and data[i+1] == 0x8d:
        for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
            if data[i+2] == reg:
                rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                target = i + 7 + rel
                if target == str_offset:
                    # Found a reference to the ISA error string
                    # Look for a call instruction within 32 bytes after the lea
                    for j in range(i, min(i+32, len(data)-5)):
                        if data[j] == 0xe8:
                            print(f"Found call at 0x{j:x} (after lea at 0x{i:x})")
                            data[j] = 0xc3
                            for k in range(j+1, j+5):
                                data[k] = 0x90
                            print(f"Patched call at 0x{j:x} -> ret + NOPs")
                            break

with open("/tmp/ld-linux-patched.so.2", "wb") as f:
    f.write(data)
print(f"\nPatched ld-linux written ({len(data)} bytes)")
PYEOF

echo ""
echo "=== Now build rootfs + boot ==="
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-final-patch

# Compile wlt-shell v5
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
            int lf = open("/tmp/tor.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
            printf("TOR_EXEC_ATTEMPT pid=%d\n", getpid()); fflush(stdout);
            char *av[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            execve("/usr/bin/tor", av, ev);
            printf("TOR_EXEC_FAILED errno=%d (%s)\n", errno, strerror(errno)); fflush(stdout);
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
echo "wlt-shell compiled"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v5 bin/wlt-shell
chmod +x bin/wlt-shell

mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
chmod +x usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
cp /tmp/ld-linux-patched.so.2 lib64/ld-linux-x86-64.so.2
chmod +x lib64/ld-linux-x86-64.so.2
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
LOG=/home/kushal/torpd-c3-call-ret-patch.log
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
echo "--- EXEC errors ---"
strings "$LOG" | grep "TOR_EXEC\|EXEC_FAILED\|Permission" | head -5
