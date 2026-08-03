#!/bin/bash
# C.2 FIX: Add dataport ID + size config attributes (like optiplex9020).
set -e
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes

echo "=== BEFORE: config section ==="
grep -A30 "configuration" "$CAMKES" | head -35

# Add the dataport ID + size config for vm0 and vm1
# vm0 has tor_src_dp (sender) + tor_dest_dp (receiver)
# vm1 has tor_src_dp (receiver) + tor_dest_dp (sender)
# Each dataport needs an ID (PCI device number) and size
python3 << 'PYEOF'
import os
fpath = os.environ.get('CAMKES', '')
with open(fpath, 'r') as f:
    content = f.read()

# Add dataport config after vm0.initrd_image line
old_vm0 = 'vm0.initrd_image = "rootfs.cpio";'
new_vm0 = '''vm0.initrd_image = "rootfs.cpio";
        vm0.tor_src_dp_id = 1;
        vm0.tor_src_dp_size = 4096;
        vm0.tor_dest_dp_id = 2;
        vm0.tor_dest_dp_size = 4096;'''
if old_vm0 in content and 'tor_src_dp_id' not in content:
    content = content.replace(old_vm0, new_vm0)
    print("Added vm0 dataport config")

# Add dataport config after vm1.initrd_image line
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

echo ""
echo "=== AFTER: config section ==="
grep -A35 "configuration" "$CAMKES" | head -40

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
cmake . 2>&1 | tail -3
ninja 2>&1 | tail -5

echo ""
echo "=== Boot ==="
LOG=/home/kushal/torpd-c2-dp-config.log
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
strings "$LOG" | grep "Found crossvm PCI\|vendor=0x1af4\|vendor=0x" | head -10
echo "--- WLTIOS-BOOT ---"
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell.*starting\|IPC sender\|IPC receiver" | head -10
