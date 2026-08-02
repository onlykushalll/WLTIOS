#!/bin/bash
# Check if wlt-init-stub is in the VMM binary or the guest kernel.
echo "=== wlt-init-stub in vm0.instance.bin ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin | grep -c "wlt-init\|WLTIOS Boot"
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin | grep "wlt-init\|WLTIOS Boot" | head -5

echo ""
echo "=== wlt-init-stub in vm1.instance.bin ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm1.instance.bin | grep -c "wlt-init\|WLTIOS Boot"
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm1.instance.bin | grep "wlt-init\|WLTIOS Boot" | head -5

echo ""
echo "=== wlt-init-stub in bzimage ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -c "wlt-init\|WLTIOS Boot"
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep "wlt-init\|WLTIOS Boot" | head -5

echo ""
echo "=== wlt-init-stub in rootfs.cpio ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep -c "wlt-init\|WLTIOS Boot"
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep "wlt-init\|WLTIOS Boot" | head -5

echo ""
echo "=== wlt-init-stub in rootfs_vm1.cpio ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio | grep -c "wlt-init\|WLTIOS Boot"
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio | grep "wlt-init\|WLTIOS Boot" | head -5

echo ""
echo "=== Search for wlt-init-stub.c source in entire build tree ==="
find /home/kushal/wltios-build -name "wlt-init-stub.c" 2>/dev/null
find /home/kushal/wltios-build -name "wlt_init_stub.c" 2>/dev/null
find /home/kushal/wltios-build -name "wlt-init*" 2>/dev/null | head -10

echo ""
echo "=== Check Init component source for wlt-init ==="
grep -rn "wlt-init\|WLTIOS Boot\|WLT-Shell" /home/kushal/wltios-build/projects/vm/components/Init/ 2>/dev/null | head -10

echo ""
echo "=== Check if Init component has a custom main.c ==="
head -50 /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null
