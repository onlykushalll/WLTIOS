#!/bin/bash
echo "=== cpio listing of rootfs.cpio (check /init position) ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | grep -E "^.*init$|^[^d].*\binit\b" | head -10
echo ""
echo "=== First 10 entries ==="
cpio -itv < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null | head -10
echo ""
echo "=== Check busybox for init applet ==="
cd /tmp && rm -rf bb-check && mkdir bb-check && cd bb-check
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo "--- busybox applets ---"
./bin/busybox --list 2>/dev/null | grep -E "^init$|^sh$|^ps$|^mount$" | head -10
echo ""
echo "=== Try running busybox init directly ==="
# Check if busybox has a custom init that prints "WLTIOS Boot"
strings ./bin/busybox | grep -c "WLTIOS Boot"
strings ./bin/busybox | grep "WLTIOS Boot" | head -3
echo ""
echo "=== Add init=/init to kernel cmdline ==="
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
# Add init=/init to the cmdline
sed -i 's|root=/dev/mem|root=/dev/mem init=/init|' "$CAMKES"
grep VM_GUEST_CMDLINE "$CAMKES" | head -1
