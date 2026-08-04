#!/bin/bash
LOG=/home/kushal/torpd-c2-probeall-final.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PCI scan messages ==="
strings "$LOG" | grep -i "pci 0000\|pci_bus\|pci_dev\|probe\|scan\|00:0" | head -15
echo ""
echo "=== IPC messages ==="
strings "$LOG" | grep "\[ipc\]" | head -20
echo ""
echo "=== IPC RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== PCI devices found ==="
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo ""
echo "=== VMM init ==="
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|vmm_pci\|Finalising\|mutex\|connection" | head -10
echo ""
echo "=== Insufficient memory ==="
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -3
echo ""
echo "=== Kernel cmdline ==="
strings "$LOG" | grep "Kernel command line" | head -2
echo ""
echo "=== Last 20 lines ==="
strings "$LOG" | tail -20
