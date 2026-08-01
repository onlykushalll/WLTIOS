#!/bin/bash
echo "=== WLT-SHELL ARCH ==="
file /tmp/rfs-check/bin/wlt-shell 2>/dev/null
readelf -h /tmp/rfs-check/bin/wlt-shell 2>/dev/null | grep -E "Class|Machine|type"

echo ""
echo "=== FIX CAMKES: vm1.initrd_image -> rootfs_vm1.cpio ==="
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
# Fix vm1 to use rootfs_vm1.cpio (was rootfs.cpio - same as vm0)
sed -i 's/vm1.initrd_image = "rootfs.cpio"/vm1.initrd_image = "rootfs_vm1.cpio"/' "$CAMKES"
echo "After fix:"
grep initrd_image "$CAMKES"
