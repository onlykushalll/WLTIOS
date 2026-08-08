#!/bin/bash
# Build rootfs using the vendored musl-Tor from packages/tor-musl-x86_64/
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-musl-final

echo "=== 1. Check vendored musl-Tor ==="
file "$REPO/packages/tor-musl-x86_64/tor" 2>/dev/null
readelf -d "$REPO/packages/tor-musl-x86_64/tor" 2>/dev/null | grep NEEDED
readelf -l "$REPO/packages/tor-musl-x86_64/tor" 2>/dev/null | grep interpreter
echo "--- libs available ---"
ls -la "$REPO/packages/tor-musl-x86_64/lib/" 2>/dev/null | head -15

echo ""
echo "=== 2. Check what deps are missing from vendored ==="
MUSL_TOR="$REPO/packages/tor-musl-x86_64"
for lib in $(readelf -d "$MUSL_TOR/tor" 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find "$MUSL_TOR" -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "MISSING: $lib"; else echo "OK: $lib"; fi
done

echo ""
echo "=== 3. Download missing deps (openssl3, libcap) ==="
# These need to be downloaded separately — use the wltios-rootfs64 glibc versions
# since they're just shared libs (the musl Tor will link against them)
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
echo "--- Copying missing deps from wltios-rootfs64 ---"
mkdir -p "$MUSL_TOR/lib"
cp "$TOR_SRC/lib/libssl.so.3" "$MUSL_TOR/lib/" 2>/dev/null && echo "Copied libssl.so.3"
cp "$TOR_SRC/lib/libcrypto.so.3" "$MUSL_TOR/lib/" 2>/dev/null && echo "Copied libcrypto.so.3"
cp "$TOR_SRC/lib/libcap.so.2"* "$MUSL_TOR/lib/" 2>/dev/null && echo "Copied libcap.so.2"

echo ""
echo "=== 4. Re-check deps ==="
for lib in $(readelf -d "$MUSL_TOR/tor" 2>/dev/null | grep NEEDED | sed 's/.*\[//;s/\].*//'); do
    found=$(find "$MUSL_TOR" -name "$lib" 2>/dev/null | head -1)
    if [ -z "$found" ]; then echo "STILL MISSING: $lib"; else echo "OK: $lib"; fi
done

echo ""
echo "=== 5. Build rootfs ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Use existing wlt-shell (the one already in the rootfs from last build)
# If it's not there, compile a simple one
if [ ! -f bin/wlt-shell ]; then
    echo "Compiling simple wlt-shell..."
    cat > /tmp/simple_wlt.c << 'SW'
#include <sys/mount.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#define SRC 0xa0001000
#define DST 0xa0003000
#define SZ 4096
static int fe(const char *p){struct stat s;return stat(p,&s)==0;}
static volatile char *msm(uintptr_t a){int f=open("/dev/mem",O_RDWR);if(f<0)return NULL;void*p=mmap(NULL,SZ,PROT_READ|PROT_WRITE,MAP_SHARED,f,a);close(f);return(p==MAP_FAILED)?NULL:(volatile char*)p;}
int main(void){mount("proc","/proc","proc",0,NULL);mount("sysfs","/sys","sysfs",0,NULL);mount("devtmpfs","/dev","devtmpfs",0,NULL);mount("tmpfs","/tmp","tmpfs",0,NULL);usleep(200000);
int v1=fe("/usr/bin/tor");printf("=====WLTIOS-BOOT=====\n");fflush(stdout);printf("[wlt-shell] %s starting\n",v1?"vm1":"vm0");fflush(stdout);
if(v1){printf("[tor-pd] Starting Tor...\n");fflush(stdout);mkdir("/var/lib/tor",0755);mkdir("/var/run",0755);pid_t t=fork();if(t==0){int l=open("/tmp/tor.log",O_WRONLY|O_CREAT|O_TRUNC,0644);if(l>=0){dup2(l,1);dup2(l,2);close(l);}char*a[]={"/usr/bin/tor","-f","/etc/tor/torrc",NULL};char*e[]={"PATH=/bin:/usr/bin:/sbin:/usr/sbin","HOME=/root",NULL};execve("/usr/bin/tor",a,e);_exit(127);}printf("[tor-pd] pid=%d\n",t);fflush(stdout);sleep(10);}
volatile char*s=msm(SRC),*d=msm(DST);
if(s&&d){printf("[bridge] shm OK\n");fflush(stdout);if(!v1){sleep(3);memset((void*)s,0,SZ);strcpy((char*)s,"ping");printf("[bridge] vm0: sent ping\n");fflush(stdout);for(int i=0;i<100;i++){usleep(100000);if(!strcmp((char*)d,"pong")){printf("[bridge] vm0: got pong!\n[bridge] RESULT: SUCCESS\n");fflush(stdout);break;}}}}
printf("=====WLTIOS-PS-BEGIN=====\n");fflush(stdout);int tc=0;DIR*dir=opendir("/proc");if(dir){struct dirent*e;while((e=readdir(dir))){int ip=1;for(int i=0;e->d_name[i];i++)if(e->d_name[i]<'0'||e->d_name[i]>'9'){ip=0;break;}if(!ip)continue;char cp[512];snprintf(cp,sizeof(cp),"/proc/%s/comm",e->d_name);FILE*fp=fopen(cp,"r");if(fp){char c[256];if(fgets(c,sizeof(c),fp)){int l=strlen(c);if(l>0&&c[l-1]=='\n')c[l-1]=0;printf("PID %s: %s\n",e->d_name,c);if(strstr(c,"tor"))tc++;}fclose(fp);}}closedir(dir);}
printf("=====WLTIOS-PS-END=====\n[wlt-check] tor:%d(%s)\n",tc,v1?"vm1":"vm0");fflush(stdout);
FILE*fp=fopen("/tmp/tor.log","r");if(fp){printf("=====WLTIOS-TOR-LOG=====\n");fflush(stdout);char b[512];while(fgets(b,sizeof(b),fp))printf("%s",b);fclose(fp);printf("=====WLTIOS-TOR-LOG-END=====\n");fflush(stdout);}
printf("[done]\n");fflush(stdout);execve("/bin/sh",(char*[]){"/bin/sh",NULL},(char*[]){"PATH=/bin:/usr/bin:/sbin:/usr/sbin","HOME=/root",NULL});for(;;)sleep(1);return 0;}
SW
    gcc -static -o /tmp/simple_wlt /tmp/simple_wlt.c 2>&1
    cp /tmp/simple_wlt bin/wlt-shell; chmod +x bin/wlt-shell
fi

# Copy musl-Tor + deps
mkdir -p usr/bin etc/tor var/lib/tor lib
cp "$MUSL_TOR/tor" usr/bin/tor; chmod +x usr/bin/tor
cp -a "$MUSL_TOR/lib/"* lib/ 2>/dev/null || true
# Fix musl symlinks
if [ -f lib/ld-musl-x86_64.so.1 ]; then
    ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1 2>/dev/null || true
fi

# Create torrc
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
# Copy wlt-shell from vm1 work dir
cp "$WORK/bin/wlt-shell" bin/wlt-shell 2>/dev/null; chmod +x bin/wlt-shell
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
LOG=/home/kushal/torpd-musl-final.log
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
echo "--- ISA errors ---"
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
