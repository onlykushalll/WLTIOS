#!/bin/bash
# Download Alpine .apk packages directly (no chroot needed) + extract + build.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-direct-download

echo "=== 1. Download Alpine v3.19 packages directly ==="
mkdir -p /tmp/alpine-pkgs
cd /tmp/alpine-pkgs

# Download each package directly using known URLs from v3.19
echo "--- tor ---"
timeout 20 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/community/x86_64/tor-0.4.8.13-r0.apk" -o tor.apk
ls -la tor.apk; file tor.apk | head -1

echo "--- libevent ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libevent-2.1.12-r2.apk" -o libevent.apk
ls -la libevent.apk 2>/dev/null; file libevent.apk 2>/dev/null | head -1

echo "--- openssl3 ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/openssl3-3.1.7-r0.apk" -o openssl3.apk
ls -la openssl3.apk 2>/dev/null; file openssl3.apk 2>/dev/null | head -1

echo "--- zlib ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zlib-1.3.1-r0.apk" -o zlib.apk
ls -la zlib.apk 2>/dev/null

echo "--- libseccomp ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libseccomp-2.5.5-r1.apk" -o libseccomp.apk
ls -la libseccomp.apk 2>/dev/null

echo "--- libcap ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/libcap-2.69-r1.apk" -o libcap.apk
ls -la libcap.apk 2>/dev/null

echo "--- xz-libs ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/xz-libs-5.4.6-r0.apk" -o xz-libs.apk
ls -la xz-libs.apk 2>/dev/null

echo "--- zstd-libs ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/zstd-libs-1.5.6-r0.apk" -o zstd-libs.apk
ls -la zstd-libs.apk 2>/dev/null

echo "--- musl ---"
timeout 15 curl -sL "https://dl-cdn.alpinelinux.org/alpine/v3.19/main/x86_64/musl-1.2.5-r0.apk" -o musl.apk
ls -la musl.apk 2>/dev/null

echo ""
echo "=== 2. Extract all packages ==="
mkdir -p /tmp/alpine-extract
cd /tmp/alpine-extract
for apk in /tmp/alpine-pkgs/*.apk; do
    if file "$apk" | grep -q "gzip"; then
        echo "Extracting $(basename $apk)..."
        tar xzf "$apk" 2>/dev/null || true
    else
        echo "SKIP $(basename $apk) (not gzip: $(file $apk | head -1))"
    fi
done

echo ""
echo "=== 3. Check Tor + all deps ==="
file usr/bin/tor 2>/dev/null
for lib in libz.so.1 libevent-2.1.so.7 libssl.so.3 libcrypto.so.3 liblzma.so.5 libzstd.so.1 libseccomp.so.2 libcap.so.2 libc.musl-x86_64.so.1; do
    found=$(find . -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib -> $found"; fi
done

echo ""
echo "=== 4. Build rootfs ==="
# Compile wlt-shell
cat > /tmp/wlt_s2.c << 'WSC'
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
gcc -static -o /tmp/wlt_s2 /tmp/wlt_s2.c 2>&1
echo "wlt-shell compiled"

rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null
cp /tmp/wlt_s2 bin/wlt-shell; chmod +x bin/wlt-shell

# Replace ALL libs with Alpine musl versions
rm -f lib/lib*.so* lib/ld-*
mkdir -p usr/bin etc/tor var/lib/tor lib
cp /tmp/alpine-extract/usr/bin/tor usr/bin/tor; chmod +x usr/bin/tor
cp -a /tmp/alpine-extract/lib/lib*.so* /tmp/alpine-extract/lib/ld-musl-x86_64.so.1 lib/ 2>/dev/null || true
cp -a /tmp/alpine-extract/usr/lib/lib*.so* lib/ 2>/dev/null || true
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
cp /tmp/wlt_s2 bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 5. Rebuild + boot ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

LOG=/home/kushal/torpd-alpine-direct.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 6. Results ==="
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- lib errors ---"
strings "$LOG" | grep -i "shared lib\|cannot open\|error while loading\|relocating" | head -5
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
