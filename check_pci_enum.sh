#!/bin/bash
LOG=/home/kushal/torpd-c2-dp-config2.log

echo "=== Kernel PCI enumeration messages ==="
strings "$LOG" | grep -i "pci.*scan\|pci.*bus\|pci.*device\|0000:\|pci_bus\|pci_dev" | head -20

echo ""
echo "=== Check if sysfs is mounted ==="
strings "$LOG" | grep -i "sysfs\|/sys/" | head -10

echo ""
echo "=== Check lspci output (if available) ==="
strings "$LOG" | grep -i "lspci\|00:0\|00:1" | head -10

echo ""
echo "=== Full boot around PCI init ==="
strings "$LOG" | grep -B2 -A2 "PCI" | head -30
