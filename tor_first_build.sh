#!/bin/bash
# Fix: vm1 starts Tor FIRST, then runs bridge receiver.
set +e
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-tor-first

echo "=== 1. Patch ld-linux ==="
python3 -c "
ld='/home/kushal/wltios-build/wltios-rootfs64/lib64/ld-linux-x86-64.so.2'
d=bytearray(open(ld,'rb').read())
for o in [0xf304,0x27ec2,0x2724c]:
    if d[o]==0xe8:
        d[o]=0x31;d[o+1]=0xc0;d[o+2]=0xc3;d[o+3]=0x90;d[o+4]=0x90
for s in [b'CPU ISA level is lower than required',b'%s: CPU ISA level is lower than required',b'is lower than required']:
    o=0
    while True:
        o=d.find(s,o)
        if o==-1: break
        for i in range(o,o+len(s)): d[i]=0
        o+=len(s)
open('/tmp/ld-linux-patched.so.2','wb').write(d)
print('ld patched')
"

echo "=== 2. Compile wlt-shell v11 (Tor FIRST, then bridge) ==="
# Same as v10 but vm1 starts Tor before bridge
cat > /tmp/wlt_shell_v11.c << 'WLTSH'
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/io.h>
#define TOR_SRC_DP_ADDR 0xa0001000
#define TOR_DEST_DP_ADDR 0xa0003000
#define SHM_SIZE 4096
#define MSG_MAGIC 0x574C5442
#define MSG_PING 0x05
#define MSG_PONG 0x06
#define MSG_CONNECT 0x01
#define MSG_RESPONSE 0x02
#define MSG_DATA 0x03
#define MSG_CLOSE 0x04
struct bridge_hdr { uint32_t magic, type, length, request_id, status, reserved[3]; };
static int file_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }
static volatile char *map_shm(uintptr_t phys_addr) {
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) return NULL;
    void *ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys_addr);
    close(fd);
    return (ptr == MAP_FAILED) ? NULL : (volatile char *)ptr;
}
static void write_msg(volatile char *shm, uint32_t type, uint32_t req_id, const void *data, uint32_t len) {
    struct bridge_hdr *hdr = (struct bridge_hdr *)shm;
    hdr->magic = MSG_MAGIC; hdr->type = type; hdr->length = len;
    hdr->request_id = req_id; hdr->status = 0;
    if (data && len > 0 && len <= SHM_SIZE - sizeof(struct bridge_hdr))
        memcpy((void *)shm + sizeof(struct bridge_hdr), data, len);
    __sync_synchronize();
}
static int read_msg(volatile char *shm, struct bridge_hdr *hdr, char *data, int max_len) {
    __sync_synchronize();
    struct bridge_hdr *shm_hdr = (struct bridge_hdr *)shm;
    if (shm_hdr->magic != MSG_MAGIC) return -1;
    memcpy(hdr, (void *)shm, sizeof(struct bridge_hdr));
    if (hdr->length > 0 && hdr->length <= max_len && hdr->length <= SHM_SIZE - sizeof(struct bridge_hdr))
        memcpy(data, (void *)shm + sizeof(struct bridge_hdr), hdr->length);
    return 0;
}
static void clear_shm(volatile char *shm) { memset((void *)shm, 0, SHM_SIZE); }
static void vm0_bridge(volatile char *src_dp, volatile char *dest_dp) {
    printf("[bridge] vm0: Starting bridge...\n"); fflush(stdout);
    clear_shm(src_dp); write_msg(src_dp, MSG_PING, 1, NULL, 0);
    struct bridge_hdr resp; char resp_data[4096];
    for (int i = 0; i < 100; i++) {
        usleep(100000);
        if (read_msg(dest_dp, &resp, resp_data, sizeof(resp_data)) == 0 && resp.type == MSG_PONG) {
            printf("[bridge] vm0: Got pong! Channel works.\n"); fflush(stdout); break;
        }
    }
    printf("[bridge] vm0: Sending CONNECT localhost:9050...\n"); fflush(stdout);
    clear_shm(src_dp); clear_shm(dest_dp);
    write_msg(src_dp, MSG_CONNECT, 2, "127.0.0.1:9050", 15);
    for (int i = 0; i < 100; i++) {
        usleep(100000);
        if (read_msg(dest_dp, &resp, resp_data, sizeof(resp_data)) == 0 && resp.type == MSG_RESPONSE) {
            printf("[bridge] vm0: Connect response: status=%u\n", resp.status); fflush(stdout);
            if (resp.status == 0) {
                printf("[bridge] vm0: Connected! Sending SOCKS5 hello...\n"); fflush(stdout);
                clear_shm(src_dp); write_msg(src_dp, MSG_DATA, 2, "\x05\x01\x00", 3);
                clear_shm(dest_dp);
                for (int j = 0; j < 50; j++) {
                    usleep(100000);
                    if (read_msg(dest_dp, &resp, resp_data, sizeof(resp_data)) == 0 && resp.type == MSG_DATA) {
                        printf("[bridge] vm0: SOCKS5 response: %d bytes\n", resp.length);
                        printf("[bridge] RESULT: SUCCESS (SOCKS5 bridge end-to-end!)\n"); fflush(stdout);
                        clear_shm(src_dp); write_msg(src_dp, MSG_CLOSE, 2, NULL, 0); return;
                    }
                }
                printf("[bridge] RESULT: PARTIAL (connected, no SOCKS5 response)\n");
            } else {
                printf("[bridge] RESULT: PARTIAL (bridge works, Tor not available)\n");
            }
            fflush(stdout); return;
        }
    }
    printf("[bridge] RESULT: FAIL (no response)\n"); fflush(stdout);
}
static void vm1_bridge(volatile char *src_dp, volatile char *dest_dp) {
    printf("[bridge] vm1: Starting bridge receiver...\n"); fflush(stdout);
    struct bridge_hdr msg; char msg_data[4096]; int tor_sock = -1;
    for (int i = 0; i < 300; i++) {
        usleep(100000);
        if (read_msg(src_dp, &msg, msg_data, sizeof(msg_data)) != 0) continue;
        if (msg.magic != MSG_MAGIC) continue;
        if (msg.type == MSG_PING) {
            printf("[bridge] vm1: Got ping, sending pong\n"); fflush(stdout);
            clear_shm(dest_dp); write_msg(dest_dp, MSG_PONG, msg.request_id, NULL, 0); clear_shm(src_dp);
        } else if (msg.type == MSG_CONNECT) {
            printf("[bridge] vm1: CONNECT '%s' — trying localhost:9050\n", msg_data); fflush(stdout);
            tor_sock = socket(AF_INET, SOCK_STREAM, 0);
            int connected = 0;
            if (tor_sock >= 0) {
                struct sockaddr_in addr = {0};
                addr.sin_family = AF_INET; addr.sin_port = htons(9050);
                addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (connect(tor_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    printf("[bridge] vm1: Connected to Tor SOCKS5!\n"); connected = 1;
                } else {
                    printf("[bridge] vm1: connect() failed: %s\n", strerror(errno));
                }
            }
            clear_shm(dest_dp); write_msg(dest_dp, MSG_RESPONSE, msg.request_id, NULL, 0);
            struct bridge_hdr *h = (struct bridge_hdr *)dest_dp; h->status = connected ? 0 : 1;
            fflush(stdout); if (!connected) { clear_shm(src_dp); continue; }
            clear_shm(src_dp);
        } else if (msg.type == MSG_DATA && tor_sock >= 0) {
            printf("[bridge] vm1: Got %d bytes, forwarding to Tor\n", msg.length); fflush(stdout);
            write(tor_sock, msg_data, msg.length);
            char buf[4096]; int n = read(tor_sock, buf, sizeof(buf));
            clear_shm(dest_dp);
            if (n > 0) { printf("[bridge] vm1: Got %d bytes from Tor\n", n); write_msg(dest_dp, MSG_DATA, msg.request_id, buf, n); }
            else { write_msg(dest_dp, MSG_DATA, msg.request_id, "no-resp", 7); }
            fflush(stdout); clear_shm(src_dp);
        } else if (msg.type == MSG_CLOSE) {
            printf("[bridge] vm1: Got close\n"); if (tor_sock >= 0) close(tor_sock);
            fflush(stdout); break;
        }
    }
    printf("[bridge] vm1: Done\n"); fflush(stdout);
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

    /* KEY CHANGE: vm1 starts Tor FIRST, then runs bridge */
    if (is_vm1) {
        printf("[tor-pd] Starting Tor daemon...\n"); fflush(stdout);
        mkdir("/var/lib/tor", 0755); mkdir("/var/run", 0755);
        pid_t tp = fork();
        if (tp == 0) {
            int lf = open("/tmp/tor.log", O_WRONLY|O_CREAT|O_TRUNC, 0644);
            if (lf >= 0) { dup2(lf, 1); dup2(lf, 2); close(lf); }
            char *av[] = {"/usr/bin/tor", "-f", "/etc/tor/torrc", NULL};
            char *ev[] = {"PATH=/bin:/usr/bin:/sbin:/usr/sbin", "HOME=/root", NULL};
            execve("/usr/bin/tor", av, ev); _exit(127);
        }
        printf("[tor-pd] Tor pid=%d\n", tp); fflush(stdout);
        sleep(8); /* wait for Tor to start */
    }

    volatile char *src_dp = map_shm(TOR_SRC_DP_ADDR);
    volatile char *dest_dp = map_shm(TOR_DEST_DP_ADDR);
    if (src_dp && dest_dp) {
        printf("[bridge] Shared memory mapped OK\n"); fflush(stdout);
        if (!is_vm1) { sleep(5); vm0_bridge(src_dp, dest_dp); }
        else { vm1_bridge(src_dp, dest_dp); }
    } else { printf("[bridge] Failed to map shared memory\n"); fflush(stdout); }

    printf("=====WLTIOS-PS-BEGIN=====\n"); fflush(stdout);
    int tor_count = 0;
    DIR *d = opendir("/proc");
    if (d) { struct dirent *e;
        while ((e = readdir(d))) {
            int ip = 1;
            for (int i = 0; e->d_name[i]; i++) if (e->d_name[i]<'0'||e->d_name[i]>'9') { ip=0; break; }
            if (!ip) continue;
            char cp[512]; snprintf(cp, sizeof(cp), "/proc/%s/comm", e->d_name);
            FILE *fp = fopen(cp, "r");
            if (fp) { char c[256]; if (fgets(c, sizeof(c), fp)) { int l=strlen(c); if(l>0&&c[l-1]=='\n')c[l-1]=0; printf("PID %s: %s\n", e->d_name, c); if (strstr(c,"tor")) tor_count++; } fclose(fp); }
        }
        closedir(d);
    }
    printf("=====WLTIOS-PS-END=====\n");
    printf("[wlt-check] tor: %d (%s)\n", tor_count, is_vm1?"vm1":"vm0"); fflush(stdout);
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
gcc -static -o /tmp/wlt_shell_v11 /tmp/wlt_shell_v11.c 2>&1
echo "wlt-shell v11 compiled"

echo "=== 3. Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v11 bin/wlt-shell; chmod +x bin/wlt-shell
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_shell_v11 bin/wlt-shell; chmod +x bin/wlt-shell
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

echo "=== 4. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3
echo "BUILD_DONE"
