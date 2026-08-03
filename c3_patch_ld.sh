#!/bin/bash
# C.3-fix v5: Patch ld-linux to skip the ISA level check.
# The check is in the dynamic linker (ld-linux-x86-64.so.2).
# We find the error string, find the code that references it,
# and NOP out the conditional jump so the check always passes.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-patched-ld

echo "=== 1. Find ISA check in ld-linux ==="
LD_SRC="$TOR_SRC/lib64/ld-linux-x86-64.so.2"
echo "Source ld-linux: $(file "$LD_SRC")"
echo "ISA string offset:"
strings -t x "$LD_SRC" | grep "CPU ISA level\|ISA level" | head -3

echo ""
echo "=== 2. Find the check function ==="
# Disassemble around the string reference
objdump -d "$LD_SRC" 2>/dev/null | grep -B20 "CPU ISA" | head -25

echo ""
echo "=== 3. Patch: find the conditional jump and NOP it ==="
# The error string is at a known offset. Find the code that loads
# this string address (lea rdi, [rip + offset]) and NOP the
# conditional jump before it.
python3 << 'PYEOF'
import subprocess, re

ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"

# Read the binary
with open(ld_path, "rb") as f:
    data = bytearray(f.read())

# Find the error string
error_str = b"CPU ISA level is lower than required"
str_offset = data.find(error_str)
print(f"Error string at offset: 0x{str_offset:x}")

# Also find "is lower than required" (partial match for variants)
str_offset2 = data.find(b"is lower than required")
print(f"'is lower than required' at offset: 0x{str_offset2:x}")

# Find all references to this string (lea instructions that load the address)
# In x86-64, lea rdi, [rip + offset] is encoded as: 48 8d 3d XX XX XX XX
# where XX XX XX XX is the relative offset from the next instruction
# We need to find where the code references this string

# Search for the string reference pattern
# The relative offset = str_offset - (instruction_end_offset)
# instruction_end = lea_offset + 7
# So relative_offset = str_offset - lea_offset - 7

found = []
for i in range(len(data) - 7):
    if data[i] == 0x48 and data[i+1] == 0x8d and data[i+2] == 0x3d:
        # lea rdi, [rip + rel32]
        rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
        target = i + 7 + rel
        if target == str_offset or target == str_offset2:
            found.append(i)
            print(f"Found lea rdi,[rip+0x{rel:x}] at offset 0x{i:x} -> targets 0x{target:x}")

if not found:
    print("No lea rdi references found. Trying broader search...")
    # Search for any reference pattern
    for i in range(len(data) - 7):
        if data[i] == 0x48 and data[i+1] == 0x8d:
            for reg in [0x05, 0x0d, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d]:
                if data[i+2] == reg:
                    rel = int.from_bytes(data[i+3:i+7], 'little', signed=True)
                    target = i + 7 + rel
                    if target == str_offset or target == str_offset2:
                        found.append(i)
                        print(f"Found lea at offset 0x{i:x} -> targets 0x{target:x}")

if found:
    # For each reference, look backwards for a conditional jump (jcc)
    # and replace it with NOPs
    for ref_offset in found:
        # Look 32 bytes before the lea for a conditional jump
        for j in range(ref_offset - 32, ref_offset - 2):
            if j < 0:
                continue
            byte = data[j]
            # Conditional jumps: 0x0F 8X (2-byte jcc near) or 7X (1-byte jcc short)
            if byte == 0x0F and j + 1 < len(data):
                next_byte = data[j+1]
                if 0x80 <= next_byte <= 0x8F:
                    # 2-byte jcc near (6 bytes total: 0F 8X XX XX XX XX)
                    print(f"Found jcc near at 0x{j:x} (0x{byte:02x} 0x{next_byte:02x})")
                    # NOP it out (6 bytes of 0x90)
                    for k in range(6):
                        data[j + k] = 0x90
                    print(f"NOPed 6 bytes at 0x{j:x}")
                    break
            elif 0x70 <= byte <= 0x7F:
                # 1-byte jcc short (2 bytes total)
                print(f"Found jcc short at 0x{j:x} (0x{byte:02x})")
                # NOP it out (2 bytes)
                data[j] = 0x90
                data[j+1] = 0x90
                print(f"NOPed 2 bytes at 0x{j:x}")
                break

    # Write patched binary
    patched_path = "/tmp/ld-linux-patched.so.2"
    with open(patched_path, "wb") as f:
        f.write(data)
    print(f"Patched ld-linux written to {patched_path}")
    print(f"Size: {len(data)} bytes")
else:
    print("ERROR: Could not find string reference in code.")
PYEOF

echo ""
echo "=== 4. Build rootfs with patched ld-linux ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Copy wlt-shell v4 (with GLIBC_TUNABLES)
gcc -static -o /tmp/wlt_shell_v4 -x c - << 'WLTSH'
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
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
            char *av[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            int lf = open("/tmp/tor.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
            execve("/usr/bin/tor", av, ev);
            _exit(127);
        }
        printf("[tor-pd] Tor started (pid=%d)\n", tp); fflush(stdout);
        sleep(10);
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
WLTSH
cp /tmp/wlt_shell_v4 bin/wlt-shell
chmod +x bin/wlt-shell

# Copy Tor + deps from wltios-rootfs64
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true

# Use PATCHED ld-linux
if [ -f /tmp/ld-linux-patched.so.2 ]; then
    cp /tmp/ld-linux-patched.so.2 lib64/ld-linux-x86-64.so.2
    echo "Using PATCHED ld-linux"
else
    cp "$TOR_SRC/lib64/ld-linux-x86-64.so.2" lib64/ld-linux-x86-64.so.2
    echo "Using original ld-linux (patch failed)"
fi
rm -f lib/ld-linux-x86-64.so.2
ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v4 bin/wlt-shell
chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo "=== 5. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo "=== 6. Boot ==="
LOG=/home/kushal/torpd-c3-patched-ld.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== 7. Results ==="
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
