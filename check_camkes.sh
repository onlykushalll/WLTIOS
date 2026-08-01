#!/bin/bash
echo "=== CAMKES ASSEMBLY (WSL build tree) ==="
grep -E "initrd_image|kernel_image|guest_ram" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>/dev/null
echo ""
echo "=== WLT-SHELL BINARY ==="
cd /tmp && rm -rf rfs-check && mkdir rfs-check && cd rfs-check
cpio -idm < /home/kushal/wltios-build/rootfs-wlt.cpio 2>/dev/null
echo "--- init script ---"
cat init 2>/dev/null | head -20
echo "--- WLT-Shell ---"
find . -name "*wlt*shell*" -o -name "WLT-Shell" -o -name "wlt-shell" 2>/dev/null | head
file usr/bin/wlt-shell 2>/dev/null || echo "no wlt-shell in usr/bin"
file bin/sh 2>/dev/null
ls -la bin/sh bin/busybox 2>/dev/null
echo "--- all binaries in usr/bin ---"
ls usr/bin/ 2>/dev/null | head -20
