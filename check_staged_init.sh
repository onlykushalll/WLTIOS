#!/bin/bash
echo "=== Staged rootfs.cpio ==="
cd /tmp && rm -rf check-staged-final && mkdir check-staged-final && cd check-staged-final
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
echo "--- /init exists? ---"
ls -la init 2>/dev/null || echo "NO /init"
echo "--- /init content ---"
cat init 2>/dev/null | head -5
echo "--- /bin/wlt-shell ---"
ls -la bin/wlt-shell 2>/dev/null || echo "NO /bin/wlt-shell"
echo "--- /bin/sh ---"
ls -la bin/sh 2>/dev/null
echo "--- /sbin/init ---"
ls -la sbin/init 2>/dev/null
echo ""
echo "=== Staged rootfs_vm1.cpio ==="
cd /tmp && rm -rf check-staged-vm1 && mkdir check-staged-vm1 && cd check-staged-vm1
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio 2>/dev/null
echo "--- /init exists? ---"
ls -la init 2>/dev/null || echo "NO /init"
echo "--- /init content ---"
cat init 2>/dev/null | head -5
echo "--- /usr/bin/tor exists? ---"
ls -la usr/bin/tor 2>/dev/null || echo "NO tor"
echo ""
echo "=== Source rootfs files ==="
ls -la /home/kushal/wltios-build/rootfs_vm0_notor64.cpio /home/kushal/wltios-build/rootfs_vm1_tor64.cpio
echo "=== fserv files ==="
ls -la /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/
