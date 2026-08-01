#!/bin/bash
set +e
echo ===TOR-DEPS===
cd /tmp && rm -rf rfs-m && mkdir rfs-m && cd rfs-m
cpio -idm < /home/kushal/wltios-build/rootfs-wlt-full.cpio 2>/dev/null
# Check tor shared lib deps using readelf (works without running the binary)
echo ---tor-NEEDED---
readelf -d usr/bin/tor 2>/dev/null | grep NEEDED
echo ---tor-interp---
readelf -l usr/bin/tor 2>/dev/null | grep interpreter
echo ---libs-present---
for lib in $(readelf -d usr/bin/tor 2>/dev/null | grep NEEDED | sed "s/.*\[//;s/\].*//"); do
  found=$(find lib lib64 usr/lib -name "$lib" 2>/dev/null | head -1)
  if [ -z "$found" ]; then echo MISSING: $lib; else echo OK: $lib -> $found; fi
done
echo
echo ===BUILD-FSERV-FILES===
ls -la /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/ 2>/dev/null
echo
echo ===ROOTFS-VM1-EXISTS===
find /home/kushal/wltios-build -name rootfs_vm1.cpio 2>/dev/null
echo
echo ===TOR-INIT-SCRIPT===
ls etc/init.d/S99* 2>/dev/null
cat etc/init.d/S99tor 2>/dev/null | head -30
echo
echo ===TORRC===
cat etc/tor/torrc 2>/dev/null | head -20
