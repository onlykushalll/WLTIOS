#!/bin/bash
# B.3: Real VM-to-VM IPC ping/pong using direct memory-mapped BAR access.
# The crossvm PCI devices expose the shared memory at:
#   vm0: BAR1 of dev 00:01.0 = 0xa0001000 (tor_src_dp, shared with vm1)
#        BAR1 of dev 00:02.0 = 0xa0003000 (tor_dest_dp, shared with vm1)
#   vm1: same physical addresses (shared via seL4SharedDataWithCaps)
#
# Protocol:
#   vm0 writes "ping" to 0xa0001000 (tor_src_dp)
#   vm1 reads "ping" from 0xa0001000 (same physical memory)
#   vm1 writes "pong" to 0xa0003000 (tor_dest_dp)
#   vm0 reads "pong" from 0xa0003000 (same physical memory)
set +e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-ipc-pong

echo "=== Compile wlt-shell v8 (with IPC ping/pong via direct memory) ==="
cat > /tmp/wlt_shell_v8.c << 'WLTSH'
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/io.h>

/* Cross-VM shared memory addresses (from PCI BAR1 scan):
 *   tor_src_dp (BAR1 of 00:01.0) = 0xa0001000 (vm0 writes, vm1 reads)
 *   tor_dest_dp (BAR1 of 00:02.0) = 0xa0003000 (vm1 writes, vm0 reads)
 * The BAR address has bit 0 set (0xa0001008 = 0xa0001000 + 8 = memory type flags)
 * The actual physical address is 0xa0001000 (mask off the low 4 bits).
 */
#define TOR_SRC_DP_ADDR  0xa0001000  /* vm0->vm1 shared memory */
#define TOR_DEST_DP_ADDR 0xa0003000  /* vm1->vm0 shared memory */
#define SHM_SIZE 4096

static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

static volatile char *map_shm(uintptr_t phys_addr) {
    /* Map physical memory via /dev/mem */
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        printf("[ipc] Cannot open /dev/mem: %s\n", strerror(errno));
        return NULL;
    }
    void *ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
    close(fd);
    if (ptr == MAP_FAILED) {
        printf("[ipc] mmap(0x%lx) failed: %s\n", phys_addr, strerror(errno));
        return NULL;
    }
    return (volatile char *)ptr;
}

int main(void) {
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    usleep(200000);

    int is_vm1 = file_exists("/usr/bin/tor");
    printf("=====WLTIOS-BOOT=====\n"); fflush(stdout);
    printf("[wlt-shell] %s starting\n", is_vm1 ? "vm1" : "vm0"); fflush(stdout);

    /* Map the shared memory regions */
    volatile char *src_dp = map_shm(TOR_SRC_DP_ADDR);
    volatile char *dest_dp = map_shm(TOR_DEST_DP_ADDR);

    if (!src_dp || !dest_dp) {
        printf("[ipc] Failed to map shared memory\n"); fflush(stdout);
    } else {
        printf("[ipc] Shared memory mapped: src=0x%lx dest=0x%lx\n",
               TOR_SRC_DP_ADDR, TOR_DEST_DP_ADDR);
        fflush(stdout);

        if (!is_vm1) {
            /* vm0: SENDER — write "ping", wait for "pong" */
            printf("[ipc] vm0: Writing 'ping' to src_dp...\n"); fflush(stdout);
            memset((void *)src_dp, 0, SHM_SIZE);
            strcpy((char *)src_dp, "ping");

            printf("[ipc] vm0: Waiting for 'pong' in dest_dp...\n"); fflush(stdout);
            for (int i = 0; i < 200; i++) {
                usleep(100000);  /* 100ms */
                if (strcmp((char *)dest_dp, "pong") == 0) {
                    printf("[ipc] vm0: Received 'pong'! PING->PONG SUCCESS!\n");
                    printf("[ipc] RESULT: SUCCESS\n");
                    fflush(stdout);
                    break;
                }
                if (i % 20 == 19) {
                    printf("[ipc] vm0: Still waiting... dest_dp='%.20s'\n", (char *)dest_dp);
                    fflush(stdout);
                }
            }
            if (strcmp((char *)dest_dp, "pong") != 0) {
                printf("[ipc] vm0: RESULT: TIMEOUT (no pong)\n");
                fflush(stdout);
            }
        } else {
            /* vm1: RECEIVER — wait for "ping", write "pong" */
            printf("[ipc] vm1: Waiting for 'ping' in src_dp...\n"); fflush(stdout);
            for (int i = 0; i < 200; i++) {
                usleep(100000);
                if (strcmp((char *)src_dp, "ping") == 0) {
                    printf("[ipc] vm1: Received 'ping'! Writing 'pong' to dest_dp...\n");
                    fflush(stdout);
                    strcpy((char *)dest_dp, "pong");
                    sleep(2);
                    printf("[ipc] vm1: RESULT: SUCCESS\n");
                    fflush(stdout);
                    break;
                }
                if (i % 20 == 19) {
                    printf("[ipc] vm1: Still waiting... src_dp='%.20s'\n", (char *)src_dp);
                    fflush(stdout);
                }
            }
            if (strcmp((char *)src_dp, "ping") != 0) {
                printf("[ipc] vm1: RESULT: TIMEOUT (no ping)\n");
                fflush(stdout);
            }
        }
    }

    /* Process listing */
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
    printf("[done]\n"); fflush(stdout);

    char *av[] = {"/bin/sh", NULL};
    char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", av, ev);
    for(;;) sleep(1);
    return 0;
}
WLTSH
gcc -static -o /tmp/wlt_shell_v8 /tmp/wlt_shell_v8.c 2>&1
echo "wlt-shell v8 compiled: $(file /tmp/wlt_shell_v8 | grep -o 'ELF 64-bit')"

echo ""
echo "=== Patch ld-linux ==="
python3 << 'PYEOF'
ld_path = "/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2"
with open(ld_path, "rb") as f:
    data = bytearray(f.read())
for offset in [0xf304, 0x27ec2, 0x2724c]:
    if data[offset] == 0xe8:
        data[offset] = 0xc3
        for k in range(offset+1, offset+5): data[k] = 0x90
for s in [b"CPU ISA level is lower than required", b"%s: CPU ISA level is lower than required", b"is lower than required"]:
    offset = 0
    while True:
        offset = data.find(s, offset)
        if offset == -1: break
        for i in range(offset, offset + len(s)): data[i] = 0x00
        offset += len(s)
with open("/tmp/ld-linux-patched.so.2", "wb") as f: f.write(data)
print("ld-linux patched")
PYEOF

echo ""
echo "=== Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v8 bin/wlt-shell; chmod +x bin/wlt-shell

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v8 bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
mkdir -p usr/bin etc/tor usr/share/tor var/lib/tor lib64
cp "$TOR_SRC/usr/bin/tor" usr/bin/tor; chmod +x usr/bin/tor
cp "$TOR_SRC/etc/tor/torrc" etc/tor/torrc 2>/dev/null || true
cp -r "$TOR_SRC/usr/share/tor/"* usr/share/tor/ 2>/dev/null || true
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true
cp /tmp/ld-linux-patched.so.2 lib64/ld-linux-x86-64.so.2; chmod +x lib64/ld-linux-x86-64.so.2
rm -f lib/ld-linux-x86-64.so.2; ln -s ../lib64/ld-linux-x86-64.so.2 lib/ld-linux-x86-64.so.2
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-c2-ipc-pong.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== RESULTS ==="
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -30
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- VMs booted ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5
