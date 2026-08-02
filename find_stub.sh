#!/bin/bash
# Find the wlt-init-stub binary in the rootfs and replace it.
cd /tmp && rm -rf rfs-find && mkdir rfs-find && cd rfs-find
cpio -idm < /home/kushal/wltios-build/rootfs_vm0_notor64.cpio 2>/dev/null

echo "=== Find wlt-init-stub binary ==="
find . -name "*wlt*init*" -o -name "*wlt*stub*" 2>/dev/null
echo ""
echo "=== Find any binary containing 'WLTIOS Boot' ==="
for f in $(find . -type f -executable 2>/dev/null); do
    if strings "$f" 2>/dev/null | grep -q "WLTIOS Boot"; then
        echo "FOUND: $f ($(file "$f" | grep -o 'ELF [0-9]*-bit[^,]*'))"
    fi
done
echo ""
echo "=== Check /init ==="
file init 2>/dev/null
ls -la init 2>/dev/null
echo ""
echo "=== Check /sbin/ and /bin/ for wlt binaries ==="
find bin sbin usr/bin usr/sbin -name "*wlt*" 2>/dev/null
echo ""
echo "=== Check /etc/init.d/ ==="
ls etc/init.d/ 2>/dev/null
echo ""
echo "=== Content of S90camkes_init ==="
cat etc/init.d/S90camkes_init 2>/dev/null | head -20
