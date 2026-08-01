#!/bin/bash
set +e
echo ===UCLIBC-ROOTFS===
cd /tmp && rm -rf rfs-u && mkdir rfs-u && cd rfs-u
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo arch-init:; file init 2>/dev/null
echo arch-busybox:; file bin/busybox 2>/dev/null
echo libc:; ls lib/libc* lib/ld* 2>/dev/null | head -5
echo tor:; ls -la usr/bin/tor 2>/dev/null || echo NO-TOR
echo
echo ===MUSL-ROOTFS===
cd /tmp && rm -rf rfs-m && mkdir rfs-m && cd rfs-m
cpio -idm < /home/kushal/wltios-build/rootfs-wlt-full.cpio 2>/dev/null
echo arch-init:; file init 2>/dev/null
echo arch-tor:; file usr/bin/tor 2>/dev/null
echo libc:; ls lib/libc* lib/ld* 2>/dev/null | head -5
echo tor-size:; ls -la usr/bin/tor 2>/dev/null
echo
echo ===CAMKES-ASM===
find /home/kushal/wltios-build/projects -name *.camkes -path *tor* 2>/dev/null
grep -rln vm1.rootfs /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head
grep -i assembly /home/kushal/wltios-build/build-tor-pd-vm1/CMakeCache.txt 2>/dev/null | head
echo
echo ===TOOLS===
dpkg -l 2>/dev/null | grep -E libevent-dev|libssl-dev|zlib1g-dev|libseccomp-dev | awk "{print \$2, \$3}"
ls /usr/lib/x86_64-linux-gnu/libevent.a /usr/lib/x86_64-linux-gnu/libssl.a /usr/lib/x86_64-linux-gnu/libz.a 2>/dev/null
gcc --version 2>/dev/null | head -1
echo nproc:; nproc
