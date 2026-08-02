#!/bin/bash
# C.1: Trace where "=== WLTIOS Boot ===" comes from in the VMM.
cd /home/kushal/wltios-build

echo "=== Search VMM source for boot messages ==="
grep -rn "WLTIOS Boot\|wlt-init\|WLT-Shell\|WLT-Init-Stub" \
    projects/vm/components/Init/src/ \
    projects/vm-examples/apps/x86/tor_pd_vm1/ \
    2>/dev/null | head -20

echo ""
echo "=== Search ALL .c files in build tree ==="
grep -rn "WLTIOS Boot" projects/ 2>/dev/null | grep -v ".o:" | head -10

echo ""
echo "=== Check if message is in the rootfs init (busybox) ==="
cd /tmp && rm -rf rfs-check2 && mkdir rfs-check2 && cd rfs-check2
cpio -idm < /home/kushal/wltios-build/rootfs_vm0_notor64.cpio 2>/dev/null
echo "--- /init content ---"
cat init 2>/dev/null
echo ""
echo "--- grep for WLTIOS in rootfs ---"
grep -r "WLTIOS Boot" . 2>/dev/null | head -5
echo ""
echo "--- grep for wlt-shell in rootfs ---"
grep -r "wlt-shell\|WLT-Shell" . 2>/dev/null | head -5
echo ""
echo "--- /bin/wlt-shell ---"
file bin/wlt-shell 2>/dev/null
cat bin/wlt-shell 2>/dev/null | head -5

echo ""
echo "=== Check if message is in bzimage (kernel built-in) ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "WLTIOS\|wlt-init\|WLT-Shell" | head -5

echo ""
echo "=== Check if message is in vm0.instance.bin (VMM) ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin | grep -i "WLTIOS\|wlt-init\|WLT-Shell" | head -5
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm1.instance.bin | grep -i "WLTIOS\|wlt-init\|WLT-Shell" | head -5
