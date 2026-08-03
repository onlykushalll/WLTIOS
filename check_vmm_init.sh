#!/bin/bash
# Check if the VMM calls camkes_cross_vm_connections_init during boot.
LOG=/home/kushal/torpd-c2-dp-config2.log

echo "=== VMM init messages ==="
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|connection\|dataport\|pci.*init\|vmm.*init" | head -20

echo ""
echo "=== Check if S90camkes_init runs ==="
strings "$LOG" | grep -i "camkes_init\|cross_vm\|event_init\|dataport" | head -10

echo ""
echo "=== Check VMM (capdl-loader) boot messages ==="
strings "$LOG" | grep -i "vm0\.\|vm1\.\|Init0\|Init1\|cross_vm" | head -15

echo ""
echo "=== Check for VMM errors ==="
strings "$LOG" | grep -i "error\|fail\|assert\|panic" | grep -v "^\[    " | head -10
