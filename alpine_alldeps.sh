#!/bin/bash
# Extract musl libs from Alpine minirootfs + use the existing musl-Tor.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-alpine-libs

echo "=== 1. Compile wlt-shell ==="
cat > /tmp/wlt_s.c << 'WSC'
#include <sys/mount.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#define SRC 0xa0001000
#define DST 0xa0003000
#define SZ 4096
static int fe(const char *p){struct stat s;return stat(p,&s)==0;}
static volatile char *msm(uintptr_t a){int f=open("/dev/mem",O_RDWR);if(f<0)return NULL;void*p=mmap(NULL,SZ,PROT_READ|PROT_WRITE,MAP_SHARED,f,a);close(f);return(p==MAP_FAILED)?NULL:(volatile char*)p;}
int main(void){
mount("proc","/proc","proc",0,NULL);mount("sysfs","/sys","sysfs",0,NULL);mount("devtmpfs","/dev","devtmpfs",0,NULL);mount("tmpfs","/tmp","tmpfs",0,NULL);usleep(200000);
int v1=fe("/usr/bin/tor");
printf("=====WLTIOS-BOOT=====\n");fflush(stdout);
printf("[wlt-shell] %s starting\n",v1?"vm1":"vm0");fflush(stdout);
if(v1){
  printf("[tor-pd] Starting Tor daemon...\n");fflush(stdout);
  mkdir("/var/lib/tor",0755);mkdir("/var/run",0755);
  pid_t t=fork();
  if(t==0){int l=open("/tmp/tor.log",O_WRONLY|O_CREAT|O_TRUNC,0644);if(l>=0){dup2(l,1);dup2(l,2);close(l);}char*a[]={"/usr/bin/tor","-f","/etc/tor/torrc",NULL};char*e[]={"PATH=/bin:/usr/bin:/sbin:/usr/sbin","HOME=/root",NULL};execve("/usr/bin/tor",a,e);_exit(127);}
  printf("[tor-pd] Tor pid=%d\n",t);fflush(stdout);
  sleep(10);
}
volatile char*s=msm(SRC),*d=msm(DST);
if(s&&d){
  printf("[bridge] shm mapped OK\n");fflush(stdout);
  if(!v1){sleep(3);memset((void*)s,0,SZ);strcpy((char*)s,"ping");printf("[bridge] vm0: sent ping\n");fflush(stdout);for(int i=0;i<100;i++){usleep(100000);if(!strcmp((char*)d,"pong")){printf("[bridge] vm0: got pong!\n[bridge] RESULT: SUCCESS\n");fflush(stdout);break;}}}
  else{printf("[bridge] vm1: waiting for ping\n");fflush(stdout);for(int i=0;i<200;i++){usleep(100000);if(!strcmp((char*)s,"ping")){printf("[bridge] vm1: got ping, sending pong\n");fflush(stdout);strcpy((char*)d,"pong");sleep(2);printf("[bridge] vm1: RESULT: SUCCESS\n");fflush(stdout);break;}}}
}
printf("=====WLTIOS-PS-BEGIN=====\n");fflush(stdout);
int tc=0;DIR*dir=opendir("/proc");if(dir){struct dirent*e;while((e=readdir(dir))){int ip=1;for(int i=0;e->d_name[i];i++)if(e->d_name[i]<'0'||e->d_name[i]>'9'){ip=0;break;}if(!ip)continue;char cp[512];snprintf(cp,sizeof(cp),"/proc/%s/comm",e->d_name);FILE*fp=fopen(cp,"r");if(fp){char c[256];if(fgets(c,sizeof(c),fp)){int l=strlen(c);if(l>0&&c[l-1]=='\n')c[l-1]=0;printf("PID %s: %s\n",e->d_name,c);if(strstr(c,"tor"))tc++;}fclose(fp);}}closedir(dir);}
printf("=====WLTIOS-PS-END=====\n[wlt-check] tor:%d(%s)\n",tc,v1?"vm1":"vm0");fflush(stdout);
FILE*fp=fopen("/tmp/tor.log","r");if(fp){printf("=====WLTIOS-TOR-LOG=====\n");fflush(stdout);char b[512];while(fgets(b,sizeof(b),fp))printf("%s",b);fclose(fp);printf("=====WLTIOS-TOR-LOG-END=====\n");fflush(stdout);}
printf("[done]\n");fflush(stdout);
execve("/bin/sh",(char*[]){"/bin/sh",NULL},(char*[]){"PATH=/bin:/usr/bin:/sbin:/usr/sbin","HOME=/root",NULL});for(;;)sleep(1);return 0;}
WSC
gcc -static -o /tmp/wlt_s /tmp/wlt_s.c 2>&1
echo "wlt-shell: $(file /tmp/wlt_s | grep -o 'ELF 64-bit')"

echo ""
echo "=== 2. Extract Alpine minirootfs for musl libs ==="
if [ ! -f /tmp/alpine-minirootfs.tar.gz ]; then
    echo "Downloading Alpine minirootfs..."
    timeout 30 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/x86_64/alpine-minirootfs-3.19.7-x86_64.tar.gz" -o /tmp/alpine-minirootfs.tar.gz
fi
mkdir -p /tmp/alpine-rootfs
cd /tmp/alpine-rootfs
tar xzf /tmp/alpine-minirootfs.tar.gz 2>/dev/null
echo "Alpine rootfs extracted"
ls lib/ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null

echo ""
echo "=== 3. Install Tor in Alpine rootfs via apk ==="
cp /etc/resolv.conf etc/resolv.conf 2>/dev/null || true
# Use the Alpine rootfs's own apk to install tor + deps
chroot /tmp/alpine-rootfs /bin/sh -c "
apk update 2>/dev/null
apk add tor libevent openssl3 zlib libseccomp libcap xz-libs zstd-libs 2>&1 | tail -10
" 2>&1 | tail -15

echo ""
echo "=== 4. Check Tor + deps in Alpine rootfs ==="
file /tmp/alpine-rootfs/usr/bin/tor 2>/dev/null
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.musl-x86_64.so.1; do
    found=$(find /tmp/alpine-rootfs -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib"; fi
done

echo ""
echo "=== 5. Build rootfs with ALL Alpine musl libs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_s bin/wlt-shell; chmod +x bin/wlt-shell

# Replace ALL libs with Alpine musl versions
rm -f lib/lib*.so* lib/ld-*
mkdir -p usr/bin etc/tor var/lib/tor lib
# Copy Tor binary
cp /tmp/alpine-rootfs/usr/bin/tor usr/bin/tor; chmod +x usr/bin/tor
# Copy ALL musl libs from Alpine rootfs
cp -a /tmp/alpine-rootfs/lib/lib*.so* /tmp/alpine-rootfs/lib/ld-musl-x86_64.so.1 lib/ 2>/dev/null || true
cp -a /tmp/alpine-rootfs/usr/lib/lib*.so* lib/ 2>/dev/null || true
# Fix symlinks
ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null || true

cat > etc/tor/torrc << 'TORRC'
DataDirectory /var/lib/tor
Log notice file /tmp/tor.log
SocksPort 9050
ExitPolicy reject *:*
User root
TORRC

# vm0
rm -rf "$WORK/vm0"; mkdir -p "$WORK/vm0"; cd "$WORK/vm0"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_s bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== 7. Boot (90s) ==="
LOG=/home/kushal/torpd-alpine-alldeps.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 8. Results ==="
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- shared lib errors ---"
strings "$LOG" | grep -i "shared lib\|cannot open\|error while loading\|relocating" | head -5
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
