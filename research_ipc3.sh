#!/bin/bash
# Research part 3: Find the proven StringReverse IPC pattern + connection base address.
echo "=== 1. Find StringReverse example ==="
find /home/kushal/wltios-build/projects -name "StringReverse*" -type d 2>/dev/null | head -3
find /home/kushal/wltios-build/projects -path "*StringReverse*" -name "*.c" 2>/dev/null | head -10

echo ""
echo "=== 2. Read StringReverse source ==="
find /home/kushal/wltios-build/projects -path "*StringReverse*" -name "*.c" 2>/dev/null | head -1 | xargs cat 2>/dev/null | head -80

echo ""
echo "=== 3. Find the connection_base_address ==="
# Search for where connection_base_address is set in the camkes config
grep -rn "connection_base\|cross_vm_base\|0x30000000\|0x20000000" /home/kushal/wltios-build/projects/vm/components/ 2>/dev/null | head -10
grep -rn "connection_base" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/ 2>/dev/null | head -5

echo ""
echo "=== 4. Check the VM.camkes for cross_vm config ==="
grep -rn "cross_vm\|connection_base\|CROSS_VM" /home/kushal/wltios-build/projects/vm/components/VM/ 2>/dev/null | head -15

echo ""
echo "=== 5. Check the generated camkes config for connection base ==="
grep -rn "connection_base\|cross_vm\|0x30000000" /home/kushal/wltios-build/build-tor-pd-vm1/camkes-gen.cmake 2>/dev/null | head -10

echo ""
echo "=== 6. Find optiplex9020 example (the proven multi-VM pattern) ==="
find /home/kushal/wltios-build/projects -name "optiplex*" -o -name "*9020*" 2>/dev/null | head -5
find /home/kushal/wltios-build/projects -name "*.camkes" -path "*x86*" 2>/dev/null | xargs grep -l "cross_vm\|StringReverse\|Init1\|vm1" 2>/dev/null | head -5

echo ""
echo "=== 7. Read the emits_event_init / consumes_event_init (guest-side helpers) ==="
find /home/kushal/wltios-build -name "emits_event_init*" -o -name "consumes_event_init*" 2>/dev/null | head -5
file /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
# Check if emits_event_init/consumes_event_init are in the rootfs
cd /tmp && rm -rf rfs-ipc && mkdir rfs-ipc && cd rfs-ipc
cpio -idm < /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/rootfs.cpio 2>/dev/null
find . -name "*event_init*" -o -name "*crossvm*" -o -name "*ipc*" 2>/dev/null | head -10
ls usr/sbin/ 2>/dev/null | head -20
