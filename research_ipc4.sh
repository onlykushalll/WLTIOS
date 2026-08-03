#!/bin/bash
# Research part 4: Read optiplex9020 example + understand guest-side IPC helpers.
echo "=== 1. Read optiplex9020.camkes (proven multi-VM pattern) ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null | head -80

echo ""
echo "=== 2. Check guest-side IPC helper tools ==="
cd /tmp/rfs-ipc 2>/dev/null || { cd /tmp && rm -rf rfs-ipc && mkdir rfs-ipc && cd rfs-ipc && cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null; }
echo "--- dataport_read ---"
file usr/sbin/dataport_read 2>/dev/null
echo "--- dataport_write ---"
file usr/sbin/dataport_write 2>/dev/null
echo "--- emits_event_init ---"
file usr/sbin/emits_event_init 2>/dev/null
echo "--- consumes_event_init ---"
file usr/sbin/consumes_event_init 2>/dev/null
echo "--- consumes_event_wait ---"
file usr/sbin/consumes_event_wait 2>/dev/null
echo "--- emits_event_emit ---"
file usr/sbin/emits_event_emit 2>/dev/null

echo ""
echo "=== 3. Check arch of these tools ==="
file usr/sbin/dataport_read usr/sbin/consumes_event_init 2>/dev/null

echo ""
echo "=== 4. Try to understand what device they use ==="
# These tools likely use /dev/dataport* or similar
strings usr/sbin/dataport_read 2>/dev/null | grep -E "/dev/|dataport|crossvm|shm" | head -10
strings usr/sbin/consumes_event_init 2>/dev/null | grep -E "/dev/|event|crossvm|notification" | head -10

echo ""
echo "=== 5. Check if there's a kernel module for the dataport ==="
find . -name "*.ko" 2>/dev/null | head -10
strings usr/sbin/dataport_read 2>/dev/null | grep -i "module\|\.ko\|ioctl\|mmap" | head -10

echo ""
echo "=== 6. Read the S90camkes_init script (sets up the guest IPC) ==="
cat etc/init.d/S90camkes_init 2>/dev/null

echo ""
echo "=== 7. Check the camkes-vm-linux guest module source ==="
find /home/kushal/wltios-build/projects -name "dataport*" -path "*/linux/*" 2>/dev/null | head -5
find /home/kushal/wltios-build/projects -name "crossvm*" -path "*/linux/*" 2>/dev/null | head -5
