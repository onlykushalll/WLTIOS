#!/bin/bash
LOG=/home/kushal/torpd-c2-128mb.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PCI scan messages ==="
strings "$LOG" | grep -i "pci 0000\|pci_bus\|00:0\|pci.*scan\|pci.*device" | head -20
echo ""
echo "=== list_pci_devices output ==="
strings "$LOG" | grep -A5 "Listing PCI devices" | head -20
echo ""
echo "=== ALL [ipc] lines ==="
strings "$LOG" | grep "\[ipc\]" | head -30
echo ""
echo "=== RAMDISK ==="
strings "$LOG" | grep "RAMDISK" | head -5
echo ""
echo "=== Kernel PCI init ==="
strings "$LOG" | grep -i "PCI\|pci" | head -15
echo ""
echo "=== sysfs mount ==="
strings "$LOG" | grep -i "sysfs\|/sys/" | head -5
echo ""
echo "=== Last 20 lines ==="
strings "$LOG" | tail -20
