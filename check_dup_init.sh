#!/bin/bash
# C.1: Check for duplicate /init entries in the cpio (last one wins).
echo "=== All 'init' entries in rootfs.cpio ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | grep -E "^\S.*\sinit$" | head -10

echo ""
echo "=== All 'init' entries in rootfs_vm1.cpio ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio 2>/dev/null | grep -E "^\S.*\sinit$" | head -10

echo ""
echo "=== Check if wlt-init-stub is in the cpio ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | grep -i "wlt" | head -10
echo "---vm1---"
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio 2>/dev/null | grep -i "wlt" | head -10

echo ""
echo "=== Check if /sbin/init is in the cpio (busybox symlink) ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | grep "sbin/init" | head -5

echo ""
echo "=== Extract and check /init content ==="
cd /tmp && rm -rf check-dup-init && mkdir check-dup-init && cd check-dup-init
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo "--- /init file type ---"
file init
echo "--- /init size ---"
ls -la init
echo "--- /init first line ---"
head -1 init
echo "--- /init md5 ---"
md5sum init

echo ""
echo "=== Check the ORIGINAL base rootfs (rootfs-wlt.cpio) for /init ==="
cd /tmp && rm -rf check-base-init && mkdir check-base-init && cd check-base-init
cpio -idm < /home/kushal/wltios-build/rootfs-wlt.cpio 2>/dev/null
echo "--- base /init entries ---"
cpio -itv < /home/kushal/wltios-build/rootfs-wlt.cpio 2>/dev/null | grep -E "^\S.*\sinit$" | head -10
echo "--- base /init file type ---"
file init 2>/dev/null
ls -la init 2>/dev/null
