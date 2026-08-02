#!/bin/bash
# C.1: Check the actual kernel cmdline the guest receives.
LOG=/home/kushal/torpd-c1-process-listing.log

echo "=== Actual kernel cmdline in boot log ==="
strings "$LOG" | grep -i "Kernel command line\|Command line:" | head -5

echo ""
echo "=== Check if init=/init is in the cmdline ==="
strings "$LOG" | grep "init=/init" | head -3

echo ""
echo "=== Check for rdinit ==="
strings "$LOG" | grep "rdinit" | head -3

echo ""
echo "=== Check what the VMM passes as cmdline ==="
# The VMM (vm0.instance.bin) constructs the guest cmdline.
# Check the camkes assembly for the actual cmdline string.
grep -A5 "VM_GUEST_CMDLINE\|kernel_cmdline" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>/dev/null | head -15

echo ""
echo "=== Check the built vm0.instance.bin for the cmdline string ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep -E "init=|rdinit=|root=/dev/mem" | head -5

echo ""
echo "=== Check if /init ran at all (look for our markers) ==="
strings "$LOG" | grep "BOOT-INIT-RAN\|VM0-BOOT\|VM1-BOOT" | head -5

echo ""
echo "=== Check for 'Starting init' kernel messages ==="
strings "$LOG" | grep -i "Starting init\|Failed to execute\|init=/\|rdinit=" | head -10

echo ""
echo "=== Full boot sequence around init ==="
strings "$LOG" | grep -B2 -A2 "Freeing unused kernel\|Starting init\|execve\|init=" | head -30
