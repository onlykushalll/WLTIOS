#!/bin/bash
LOG=/home/kushal/torpd-a2-dsdt-fixed.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PCI scan messages ==="
strings "$LOG" | grep -i "pci 0000\|00:0\|pci.*device\|pci.*scan\|PCI0\|_SB\|host bridge\|PNP0A03" | head -15
echo ""
echo "=== [pci] messages ==="
strings "$LOG" | grep "\[pci\]" | head -15
echo ""
echo "=== IPC ==="
strings "$LOG" | grep "\[ipc\]" | head -10
echo ""
echo "=== Kernel PCI init ==="
strings "$LOG" | grep -i "PCI\|pci" | head -20
echo ""
echo "=== VMs booted ==="
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo ""
echo "=== Last 20 lines ==="
strings "$LOG" | tail -20
