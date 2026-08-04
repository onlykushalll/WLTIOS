#!/bin/bash
LOG=/home/kushal/torpd-c2-reduced-ram.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== Insufficient memory ==="
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -3
echo ""
echo "=== IPC messages ==="
strings "$LOG" | grep "\[ipc\]" | head -25
echo ""
echo "=== IPC RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== PCI devices ==="
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo ""
echo "=== VMM boot ==="
strings "$LOG" | grep -i "Booting all\|dropped to user\|Finalising\|cross_vm\|mutex" | head -10
