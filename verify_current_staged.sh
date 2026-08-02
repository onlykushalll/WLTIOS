#!/bin/bash
# Verify the CURRENT staged rootfs has the compiled binary /init.
echo "=== Staged rootfs.cpio /init entry ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | grep -E "^\S.*\sinit$" | head -5

echo ""
echo "=== Staged rootfs_vm1.cpio /init entry ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio 2>/dev/null | grep -E "^\S.*\sinit$" | head -5

echo ""
echo "=== Source rootfs files ==="
ls -la /home/kushal/wltios-build/rootfs_vm0_notor64.cpio /home/kushal/wltios-build/rootfs_vm1_tor64.cpio

echo ""
echo "=== Extract and verify /init from staged rootfs.cpio ==="
cd /tmp && rm -rf verify-staged && mkdir verify-staged && cd verify-staged
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo "--- /init file type ---"
file init
echo "--- /init size ---"
ls -la init
echo "--- /init md5 ---"
md5sum init
echo "--- /sbin/init exists? ---"
ls -la sbin/init 2>&1
echo "--- /bin/wlt-shell exists? ---"
ls -la bin/wlt-shell 2>&1

echo ""
echo "=== Check /bin/busybox for wlt-init strings ==="
strings bin/busybox | grep -c "wlt-init\|WLTIOS Boot"
strings bin/busybox | grep "wlt-init\|WLTIOS Boot" | head -5
