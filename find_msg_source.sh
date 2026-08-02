#!/bin/bash
# Find where "=== WLTIOS Boot ===" and "wlt-init" actually come from.
echo "=== Search ALL binaries in build tree ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
for f in vm0.instance.bin vm1.instance.bin capdl-loader images/capdl-loader-image-x86_64-pc99; do
    echo "--- $f ---"
    # Use strings with exact match
    matches=$(strings "$f" 2>/dev/null | grep -c "WLTIOS Boot")
    echo "  'WLTIOS Boot' matches: $matches"
    matches2=$(strings "$f" 2>/dev/null | grep -c "wlt-init")
    echo "  'wlt-init' matches: $matches2"
    matches3=$(strings "$f" 2>/dev/null | grep -c "WLT-Shell")
    echo "  'WLT-Shell' matches: $matches3"
    if [ "$matches" -gt 0 ] || [ "$matches2" -gt 0 ] || [ "$matches3" -gt 0 ]; then
        echo "  FOUND IN: $f"
        strings "$f" | grep -i "WLTIOS\|wlt-init\|WLT-Shell" | head -5
    fi
done

echo ""
echo "=== Search bzimage ==="
strings apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -c "WLTIOS Boot"
strings apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -c "wlt-init"
strings apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -c "WLT-Shell"

echo ""
echo "=== Search rootfs.cpio (the actual staged file) ==="
strings apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep -c "WLTIOS Boot"
strings apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep -c "wlt-init"
strings apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep -c "WLT-Shell"
strings apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio | grep "WLTIOS\|wlt-init\|WLT-Shell" | head -10

echo ""
echo "=== Check rootfs_vm1.cpio ==="
strings apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio | grep -c "WLTIOS Boot"
strings apps/x86/tor_pd_vm1/fserv/files/rootfs_vm1.cpio | grep "WLTIOS\|wlt-init\|WLT-Shell" | head -10
