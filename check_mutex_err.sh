#!/bin/bash
LOG=/home/kushal/torpd-c2-dp-config2.log
echo "=== Search for mutex error ==="
strings "$LOG" | grep -i "mutex\|cross_vm_event\|initialisation" | head -10

echo ""
echo "=== Search for any VMM error messages ==="
strings "$LOG" | grep -i "error\|fail\|assert\|cross_vm\|camkes_cross\|connection" | grep -v "^\[    " | grep -v "^\[ipc\]" | head -15

echo ""
echo "=== Check if VMM boots at all (look for VMM init messages) ==="
strings "$LOG" | grep -i "vmm\|init.*guest\|Booting\|guest.*start\|Finalising" | head -10

echo ""
echo "=== Check for ZF_LOG messages ==="
strings "$LOG" | grep -i "ZF_LOG\|libsel4\|sel4.*log" | head -5

echo ""
echo "=== Full boot around 'Booting all finished' ==="
strings "$LOG" | grep -B2 -A5 "Booting all finished\|dropped to user" | head -20
