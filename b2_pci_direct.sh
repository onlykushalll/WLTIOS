#!/bin/bash
# B.2: Add direct PCI config space read to wlt-shell to test if 0xCF8/0xCFC works.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=/home/kushal/wltios-build
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-pci-direct

echo "=== Compile wlt-shell v7 (with direct PCI config read) ==="
cat > /tmp/wlt_shell_v7.c << 'WLTSH'
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
#include <sys/io.h>

static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

static void scan_pci_direct(void) {
    printf("[pci] Direct PCI config space scan via I/O ports 0xCF8/0xCFC\n");
    fflush(stdout);

    /* Request I/O port access */
    if (iopl(3) < 0) {
        printf("[pci] iopl(3) failed: %s\n", strerror(errno));
        fflush(stdout);
        return;
    }

    /* Scan bus 0, devices 0-31, function 0 */
    int found = 0;
    for (int dev = 0; dev < 32; dev++) {
        /* Build config address: enable=1, bus=0, dev, fun=0, offset=0 (vendor ID) */
        uint32_t addr = 0x80000000 | (0 << 16) | (dev << 11) | (0 << 8) | 0;
        outl(addr, 0xCF8);
        uint32_t data = inl(0xCFC);

        if (data != 0xFFFFFFFF) {
            uint16_t vendor = data & 0xFFFF;
            uint16_t device = (data >> 16) & 0xFFFF;
            printf("[pci] Found device at 00:%02x.0: vendor=0x%04x device=0x%04x\n",
                   dev, vendor, device);
            fflush(stdout);
            found++;

            /* Read class code (offset 0x08) */
            uint32_t class_addr = 0x80000000 | (0 << 16) | (dev << 11) | (0 << 8) | 0x08;
            outl(class_addr, 0xCF8);
            uint32_t class_data = inl(0xCFC);
            printf("[pci]   class=0x%06x rev=0x%02x\n",
                   (class_data >> 8) & 0xFFFFFF, class_data & 0xFF);
            fflush(stdout);

            /* Read BAR0 (offset 0x10) */
            uint32_t bar0_addr = 0x80000000 | (0 << 16) | (dev << 11) | (0 << 8) | 0x10;
            outl(bar0_addr, 0xCF8);
            uint32_t bar0 = inl(0xCFC);
            printf("[pci]   BAR0=0x%08x\n", bar0);
            fflush(stdout);

            /* Read BAR1 (offset 0x14) */
            uint32_t bar1_addr = 0x80000000 | (0 << 16) | (dev << 11) | (0 << 8) | 0x14;
            outl(bar1_addr, 0xCF8);
            uint32_t bar1 = inl(0xCFC);
            printf("[pci]   BAR1=0x%08x\n", bar1);
            fflush(stdout);
        }
    }

    if (!found) {
        printf("[pci] No PCI devices found (all returned 0xFFFFFFFF)\n");
        fflush(stdout);
    } else {
        printf("[pci] Found %d PCI device(s)\n", found);
        fflush(stdout);
    }
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

    /* Direct PCI scan */
    scan_pci_direct();

    /* Also list /sys/bus/pci/devices */
    DIR *d = opendir("/sys/bus/pci/devices");
    if (d) {
        struct dirent *e;
        printf("[pci] /sys/bus/pci/devices entries:\n");
        fflush(stdout);
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            printf("[pci]   %s\n", e->d_name);
            fflush(stdout);
        }
        closedir(d);
    } else {
        printf("[pci] Cannot open /sys/bus/pci/devices: %s\n", strerror(errno));
        fflush(stdout);
    }

    /* Process listing */
    printf("=====WLTIOS-PS-BEGIN=====\n"); fflush(stdout);
    d = opendir("/proc");
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
gcc -static -o /tmp/wlt_shell_v7 /tmp/wlt_shell_v7.c 2>&1
echo "wlt-shell v7 compiled: $(file /tmp/wlt_shell_v7 | grep -o 'ELF 64-bit')"

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
cp /tmp/wlt_shell_v7 bin/wlt-shell; chmod +x bin/wlt-shell

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v7 bin/wlt-shell; chmod +x bin/wlt-shell
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
echo "=== Rebuild ==="
cd "$BUILD/build-tor-pd-vm1"
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-pci-direct.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== Results ==="
echo "--- PCI scan messages ---"
strings "$LOG" | grep "\[pci\]" | head -20
echo "--- PCI devices via I/O ---"
strings "$LOG" | grep "Found device\|vendor=0x" | head -10
echo "--- /sys/bus/pci ---"
strings "$LOG" | grep -A5 "sys/bus/pci" | head -10
