#!/bin/bash
# Complete fix: compile wlt-shell + add ALL musl deps from Alpine + boot.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BASE=/home/kushal/wltios-build/rootfs-wlt.cpio
TOR_SRC=/home/kushal/wltios-build/wltios-rootfs64
VM0_OUT=/home/kushal/wltios-build/rootfs_vm0_notor64.cpio
VM1_OUT=/home/kushal/wltios-build/rootfs_vm1_tor64.cpio
WORK=/tmp/rootfs-musl-complete

echo "=== 1. Compile wlt-shell (64-bit static, simple version) ==="
cat > /tmp/wlt_simple.c << 'WSC'
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
  if(!v1){
    sleep(3);
    memset((void*)s,0,SZ);strcpy((char*)s,"ping");
    printf("[bridge] vm0: sent ping, waiting for pong...\n");fflush(stdout);
    for(int i=0;i<100;i++){usleep(100000);if(!strcmp((char*)d,"pong")){printf("[bridge] vm0: got pong!\n[bridge] RESULT: SUCCESS\n");fflush(stdout);break;}}
  } else {
    printf("[bridge] vm1: waiting for ping...\n");fflush(stdout);
    for(int i=0;i<200;i++){usleep(100000);if(!strcmp((char*)s,"ping")){printf("[bridge] vm1: got ping, sending pong\n");fflush(stdout);strcpy((char*)d,"pong");sleep(2);printf("[bridge] vm1: RESULT: SUCCESS\n");fflush(stdout);break;}}
  }
}
printf("=====WLTIOS-PS-BEGIN=====\n");fflush(stdout);
int tc=0;DIR*dir=opendir("/proc");
if(dir){struct dirent*e;while((e=readdir(dir))){int ip=1;for(int i=0;e->d_name[i];i++)if(e->d_name[i]<'0'||e->d_name[i]>'9'){ip=0;break;}if(!ip)continue;char cp[512];snprintf(cp,sizeof(cp),"/proc/%s/comm",e->d_name);FILE*fp=fopen(cp,"r");if(fp){char c[256];if(fgets(c,sizeof(c),fp)){int l=strlen(c);if(l>0&&c[l-1]=='\n')c[l-1]=0;printf("PID %s: %s\n",e->d_name,c);if(strstr(c,"tor"))tc++;}fclose(fp);}}
closedir(dir);}
printf("=====WLTIOS-PS-END=====\n[wlt-check] tor:%d(%s)\n",tc,v1?"vm1":"vm0");fflush(stdout);
FILE*fp=fopen("/tmp/tor.log","r");
if(fp){printf("=====WLTIOS-TOR-LOG=====\n");fflush(stdout);char b[512];while(fgets(b,sizeof(b),fp))printf("%s",b);fclose(fp);printf("=====WLTIOS-TOR-LOG-END=====\n");fflush(stdout);}
printf("[done]\n");fflush(stdout);
execve("/bin/sh",(char*[]){"/bin/sh",NULL},(char*[]){"PATH=/bin:/usr/bin:/sbin:/usr/sbin","HOME=/root",NULL});
for(;;)sleep(1);return 0;}
WSC
gcc -static -o /tmp/wlt_simple /tmp/wlt_simple.c 2>&1
echo "wlt-shell compiled: $(file /tmp/wlt_simple | grep -o 'ELF 64-bit')"

echo ""
echo "=== 2. Build rootfs with ALL deps ==="
rm -rf "$WORK"; mkdir -p "$WORK"; cd "$WORK"
cpio -idm < "$BASE" 2>/dev/null

# Replace wlt-shell with 64-bit static binary
rm -f bin/wlt-shell
cp /tmp/wlt_simple bin/wlt-shell; chmod +x bin/wlt-shell
echo "wlt-shell: $(file bin/wlt-shell | grep -o 'ELF 64-bit')"

# Copy musl-Tor binary
mkdir -p usr/bin etc/tor var/lib/tor
cp "$REPO/packages/tor-musl-x86_64/tor" usr/bin/tor; chmod +x usr/bin/tor
echo "tor: $(file usr/bin/tor | grep -o 'ELF 64-bit.*musl')"

# Copy ALL libs from wltios-rootfs64 (has all the deps)
# The musl-Tor needs musl libc, but the other deps (libssl, libcrypto, etc)
# can come from either source — the musl linker will load them
cp -a "$TOR_SRC/lib/"* lib/ 2>/dev/null || true

# Also ensure musl ld is present
if [ ! -f lib/ld-musl-x86_64.so.1 ]; then
    echo "WARNING: musl ld not in wltios-rootfs64 — checking packages/"
    if [ -f "$REPO/packages/tor-musl-x86_64/lib/ld-musl-x86_64.so.1" ]; then
        cp "$REPO/packages/tor-musl-x86_64/lib/ld-musl-x86_64.so.1" lib/
        ln -sf ld-musl-x86_64.so.1 lib/libc.musl-x86_64.so.1
        echo "Copied musl ld from packages/"
    fi
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
rm -f bin/wlt-shell
cp /tmp/wlt_simple bin/wlt-shell; chmod +x bin/wlt-shell
find . -print0 | cpio -o0 -H newc > "$VM0_OUT" 2>/dev/null
echo "vm0: $(stat -c%s "$VM0_OUT") bytes"

# vm1
cd "$WORK"
find . -path ./vm0 -prune -o -print0 | cpio -o0 -H newc > "$VM1_OUT" 2>/dev/null
echo "vm1: $(stat -c%s "$VM1_OUT") bytes"

echo ""
echo "=== 3. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
rm -rf apps/x86/tor_pd_vm1/fserv/files/*
ninja 2>&1 | tail -3

echo ""
echo "=== 4. Boot (90s) ==="
LOG=/home/kushal/torpd-musl-complete.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 5. Results ==="
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- shared lib errors ---"
strings "$LOG" | grep -i "shared lib\|cannot open\|error while loading" | head -5
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
