#!/bin/bash
# Research part 5: Understand the dataport device interface + check if kernel module exists.
echo "=== 1. Check what /dev/dataport* looks like in the guest ==="
# The camkes-vm-linux kernel module creates /dev/dataport<id> devices
# Let's search for the kernel module source
find /home/kushal/wltios-build/projects -name "*.c" -path "*/camkes-connector*" 2>/dev/null | head -10
find /home/kushal/wltios-build/projects -name "dataport*" -path "*/connector*" 2>/dev/null | head -10

echo ""
echo "=== 2. Read the dataport read.c source ==="
# The strings showed the source path
find /home/kushal/wltios-build -name "read.c" -path "*/dataport/*" 2>/dev/null | head -3
find /home/kushal/wltios-build -name "*.c" -path "*/camkes-connector*" 2>/dev/null | head -10

echo ""
echo "=== 3. Check if camkes-connector-apps exists in the build tree ==="
find /home/kushal/wltios-build -name "camkes-connector*" -type d 2>/dev/null | head -5
find /home/kushal/wltios-build -path "*/camkes-connector-apps*" -name "*.c" 2>/dev/null | head -15

echo ""
echo "=== 4. Look for the kernel module that creates /dev/dataport ==="
find /home/kushal/wltios-build -name "*.c" | xargs grep -l "dataport\|/dev/dataport" 2>/dev/null | grep -i "kernel\|module\|driver" | head -10
find /home/kushal/wltios-build -name "*.ko" 2>/dev/null | head -10

echo ""
echo "=== 5. Check the bzimage for dataport support ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "dataport\|crossvm\|camkes" | head -10

echo ""
echo "=== 6. Check if the guest kernel has the dataport driver ==="
# The camkes-vm-linux kernel patches add a dataport driver
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "dataport\|/dev/dataport\|cross_vm\|camkes_vm" | head -15

echo ""
echo "=== 7. Read the dataport_read source to understand the interface ==="
# Try to find the source
DP_SRC=$(find /home/kushal/wltios-build -name "read.c" -path "*/dataport/*" 2>/dev/null | head -1)
if [ -n "$DP_SRC" ]; then
    cat "$DP_SRC" | head -60
else
    echo "Source not found, trying alternative paths..."
    find /home/kushal/wltios-build -name "*.c" 2>/dev/null | xargs grep -l "dataport.*open\|/dev/dataport" 2>/dev/null | head -5
fi
