#!/bin/bash
# C.2 FIX: Add dataport ID + size config attributes.
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
export CAMKES

echo "=== Add dataport config ==="
python3 << 'PYEOF'
import os
fpath = os.environ['CAMKES']
with open(fpath, 'r') as f:
    content = f.read()

old_vm0 = 'vm0.initrd_image = "rootfs.cpio";'
new_vm0 = '''vm0.initrd_image = "rootfs.cpio";
        vm0.tor_src_dp_id = 1;
        vm0.tor_src_dp_size = 4096;
        vm0.tor_dest_dp_id = 2;
        vm0.tor_dest_dp_size = 4096;'''
if old_vm0 in content and 'tor_src_dp_id' not in content:
    content = content.replace(old_vm0, new_vm0)
    print("Added vm0 dataport config")

old_vm1 = 'vm1.initrd_image = "rootfs_vm1.cpio";'
new_vm1 = '''vm1.initrd_image = "rootfs_vm1.cpio";
        vm1.tor_src_dp_id = 1;
        vm1.tor_src_dp_size = 4096;
        vm1.tor_dest_dp_id = 2;
        vm1.tor_dest_dp_size = 4096;'''
if old_vm1 in content and 'vm1.tor_src_dp_id' not in content:
    content = content.replace(old_vm1, new_vm1)
    print("Added vm1 dataport config")

with open(fpath, 'w') as f:
    f.write(content)
print("Config updated")
PYEOF

echo "=== Verify config ==="
grep "tor_src_dp_id\|tor_dest_dp_id\|tor_src_dp_size\|tor_dest_dp_size" "$CAMKES" | head -10

echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
cmake . 2>&1 | tail -3
ninja 2>&1 | tail -5

echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-dp-config2.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 2G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== Results ==="
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -25
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices found ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo "--- WLTIOS-BOOT ---"
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell.*starting\|IPC sender\|IPC receiver" | head -10
