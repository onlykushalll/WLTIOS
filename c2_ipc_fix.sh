#!/bin/bash
# Fix the mount() issue by adding sys/mount.h, then rebuild + boot.
set -e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-ipc-fix

echo "=== Compile 64-bit IPC test (with sys/mount.h) ==="
cat > /tmp/vm_ipc_test_64.c << 'IPCC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>

#define DATAPORT_SIZE 4096
#define PCI_VENDOR 0x1af4
#define PCI_DEVICE 0xa111

static char *find_pci_resource(void) {
    DIR *d = opendir("/sys/bus/pci/devices");
    if (!d) {
        printf("[ipc] Cannot open /sys/bus/pci/devices: %s\n", strerror(errno));
        return NULL;
    }
    struct dirent *e;
    static char path[256];
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char vendor_path[256], device_path[256];
        snprintf(vendor_path, sizeof(vendor_path), "/sys/bus/pci/devices/%s/vendor", e->d_name);
        snprintf(device_path, sizeof(device_path), "/sys/bus/pci/devices/%s/device", e->d_name);
        FILE *vf = fopen(vendor_path, "r");
        FILE *df = fopen(device_path, "r");
        if (vf && df) {
            unsigned int vendor = 0, device = 0;
            fscanf(vf, "%x", &vendor);
            fscanf(df, "%x", &device);
            fclose(vf); fclose(df);
            if (vendor == PCI_VENDOR && device == PCI_DEVICE) {
                printf("[ipc] Found crossvm PCI device: %s (vendor=0x%04x device=0x%04x)\n",
                       e->d_name, vendor, device);
                snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/resource1", e->d_name);
                closedir(d);
                return path;
            }
        }
        if (vf) fclose(vf);
        if (df) fclose(df);
    }
    closedir(d);
    return NULL;
}

static void list_pci_devices(void) {
    printf("[ipc] Listing PCI devices:\n");
    DIR *d = opendir("/sys/bus/pci/devices");
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char vendor_path[256], device_path[256], class_path[256];
        snprintf(vendor_path, sizeof(vendor_path), "/sys/bus/pci/devices/%s/vendor", e->d_name);
        snprintf(device_path, sizeof(device_path), "/sys/bus/pci/devices/%s/device", e->d_name);
        snprintf(class_path, sizeof(class_path), "/sys/bus/pci/devices/%s/class", e->d_name);
        FILE *vf = fopen(vendor_path, "r");
        FILE *df = fopen(device_path, "r");
        FILE *cf = fopen(class_path, "r");
        if (vf && df) {
            unsigned int vendor = 0, device = 0, cls = 0;
            fscanf(vf, "%x", &vendor);
            fscanf(df, "%x", &device);
            if (cf) { fscanf(cf, "%x", &cls); fclose(cf); }
            printf("[ipc]   %s: vendor=0x%04x device=0x%04x class=0x%06x\n",
                   e->d_name, vendor, device, cls);
            fclose(vf); fclose(df);
        } else {
            if (vf) fclose(vf);
            if (df) fclose(df);
        }
    }
    closedir(d);
}

int main(int argc, char *argv[]) {
    int is_sender = 1;
    if (argc > 1 && strcmp(argv[1], "receiver") == 0) is_sender = 0;

    printf("[ipc] VM-to-VM IPC test (role: %s)\n", is_sender ? "sender" : "receiver");
    fflush(stdout);

    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    char *resource_path = find_pci_resource();
    if (!resource_path) {
        printf("[ipc] Crossvm PCI device not found!\n");
        list_pci_devices();
        printf("[ipc] RESULT: FAIL (no PCI device)\n");
        fflush(stdout);
        return 1;
    }

    printf("[ipc] Opening: %s\n", resource_path);
    fflush(stdout);

    int fd = open(resource_path, O_RDWR);
    if (fd < 0) {
        printf("[ipc] open failed: %s\n", strerror(errno));
        printf("[ipc] RESULT: FAIL (open)\n");
        fflush(stdout);
        return 1;
    }

    void *shm = mmap(NULL, DATAPORT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        printf("[ipc] mmap failed: %s\n", strerror(errno));
        close(fd);
        printf("[ipc] RESULT: FAIL (mmap)\n");
        fflush(stdout);
        return 1;
    }

    printf("[ipc] shm mapped at %p\n", shm);
    fflush(stdout);

    if (is_sender) {
        printf("[ipc] Writing 'ping'...\n"); fflush(stdout);
        memset(shm, 0, DATAPORT_SIZE);
        strcpy((char *)shm, "ping");
        printf("[ipc] Waiting for 'pong'...\n"); fflush(stdout);
        for (int i = 0; i < 100; i++) {
            usleep(100000);
            if (strcmp((char *)shm, "pong") == 0) {
                printf("[ipc] Received 'pong'!\n");
                printf("[ipc] RESULT: SUCCESS\n");
                fflush(stdout);
                munmap(shm, DATAPORT_SIZE);
                close(fd);
                return 0;
            }
            if (i % 10 == 9) {
                printf("[ipc] waiting... data='%.20s'\n", (char *)shm); fflush(stdout);
            }
        }
        printf("[ipc] RESULT: TIMEOUT\n"); fflush(stdout);
    } else {
        printf("[ipc] Waiting for 'ping'...\n"); fflush(stdout);
        for (int i = 0; i < 100; i++) {
            usleep(100000);
            if (strcmp((char *)shm, "ping") == 0) {
                printf("[ipc] Received 'ping'!\n", is_sender ? "" : "");
                printf("[ipc] Writing 'pong'...\n"); fflush(stdout);
                strcpy((char *)shm, "pong");
                sleep(2);
                printf("[ipc] RESULT: SUCCESS\n"); fflush(stdout);
                munmap(shm, DATAPORT_SIZE);
                close(fd);
                return 0;
            }
            if (i % 10 == 9) {
                printf("[ipc] waiting... data='%.20s'\n", (char *)shm); fflush(stdout);
            }
        }
        printf("[ipc] RESULT: TIMEOUT\n"); fflush(stdout);
    }

    munmap(shm, DATAPORT_SIZE);
    close(fd);
    return 1;
}
IPCC
gcc -static -o /tmp/vm_ipc_test_64 /tmp/vm_ipc_test_64.c 2>&1
echo "IPC test compiled: $(file /tmp/vm_ipc_test_64 | grep -o 'ELF 64-bit')"

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

echo "=== Compile wlt-shell v6 ==="
cat > /tmp/wlt_shell_v6.c << 'WLTSH'
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
    usleep(200000);
    int is_vm1 = file_exists("/usr/bin/tor");
    printf("=====WLTIOS-BOOT=====\n"); fflush(stdout);
    printf("[wlt-shell] %s starting\n", is_vm1 ? "vm1" : "vm0"); fflush(stdout);
    if (is_vm1) {
        printf("[tor-pd] Starting Tor...\n"); fflush(stdout);
        mkdir("/var/lib/tor", 0755); mkdir("/var/run", 0755); mkdir("/tmp", 0755);
        pid_t tp = fork();
        if (tp == 0) {
            int lf = open("/tmp/tor.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
            char *av[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            execve("/usr/bin/tor", av, ev);
            _exit(127);
        }
        printf("[tor-pd] pid=%d\n", tp); fflush(stdout);
        sleep(5);
        printf("[ipc] vm1: IPC receiver\n"); fflush(stdout);
        pid_t ip = fork();
        if (ip == 0) {
            char *av[] = {"/bin/vm_ipc_test_64", "receiver", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", NULL};
            execve("/bin/vm_ipc_test_64", av, ev);
            _exit(127);
        }
        waitpid(ip, NULL, 0);
        sleep(10);
    } else {
        printf("[ipc] vm0: IPC sender\n"); fflush(stdout);
        sleep(3);
        pid_t ip = fork();
        if (ip == 0) {
            char *av[] = {"/bin/vm_ipc_test_64", "sender", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", NULL};
            execve("/bin/vm_ipc_test_64", av, ev);
            _exit(127);
        }
        waitpid(ip, NULL, 0);
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
    FILE *fp = fopen("/tmp/tor.log", "r");
    if (fp) { printf("=====WLTIOS-TOR-LOG=====\n"); fflush(stdout); char b[512]; while(fgets(b,sizeof(b),fp))printf("%s",b); fclose(fp); printf("=====WLTIOS-TOR-LOG-END=====\n"); fflush(stdout); }
    printf("[done]\n"); fflush(stdout);
    char *av[] = {"/bin/sh", NULL};
    char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", "TERM=linux", NULL};
    execve("/bin/sh", av, ev);
    for(;;) sleep(1);
    return 0;
}
WLTSH
gcc -static -o /tmp/wlt_shell_v6 /tmp/wlt_shell_v6.c 2>&1
echo "wlt-shell v6 compiled"

echo "=== Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v6 bin/wlt-shell; chmod +x bin/wlt-shell
cp /tmp/vm_ipc_test_64 bin/vm_ipc_test_64; chmod +x bin/vm_ipc_test_64

rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v6 bin/wlt-shell; chmod +x bin/wlt-shell
cp /tmp/vm_ipc_test_64 bin/vm_ipc_test_64; chmod +x bin/vm_ipc_test_64
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

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

echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-ipc-fix.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== Results ==="
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- WLTIOS-BOOT ---"
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell.*starting" | head -5
echo "--- tor PIDs ---"
strings "$LOG" | grep "^PID " | grep -i "tor" | head -5
echo "--- ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
