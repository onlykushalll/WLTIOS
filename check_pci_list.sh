#!/bin/bash
LOG=/home/kushal/torpd-c2-ipc-fix.log
echo "=== PCI device listing from IPC test ==="
strings "$LOG" | grep -A10 "Listing PCI devices" | head -30
echo ""
echo "=== Check if /sys/bus/pci exists in guest ==="
strings "$LOG" | grep -i "pci\|Cannot open.*sys" | head -10
echo ""
echo "=== Check kernel PCI init messages ==="
strings "$LOG" | grep -i "pci\|0000:" | head -20
