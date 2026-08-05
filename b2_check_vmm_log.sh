#!/bin/bash
# B.2: Add explicit PCI scan to wlt-shell + check VMM debug messages.
set +e
LOG=/home/kushal/torpd-c2-128mb.log

echo "=== 1. Check for VMM ZF_LOGI messages ==="
strings "$LOG" | grep -i "Adding virtual PCI\|PCI scan\|PCI device discovery\|Finalising" | head -10

echo ""
echo "=== 2. Check for ALL ZF_LOG messages ==="
strings "$LOG" | grep -i "ZF_LOG\|libsel4\|vmm.*log" | head -5

echo ""
echo "=== 3. Check if VMM messages are printed at all ==="
strings "$LOG" | grep -i "Starting VM\|Guest\|VMM\|init.*guest\|Booting" | head -10

echo ""
echo "=== 4. Full boot (first 80 lines) ==="
strings "$LOG" | head -80

echo ""
echo "=== 5. Check if 0xCF8/0xCFC I/O ports are being trapped ==="
# If the guest reads 0xCF8 and the VMM handler fires, there should be
# some indication in the log. But ZF_LOGI might not be printed.
# Let's check if the VMM even gets to the PCI init.
strings "$LOG" | grep -i "PCI\|pci" | head -20
