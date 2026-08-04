#!/bin/bash
LOG=/home/kushal/torpd-c2-more-mem.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== Insufficient memory errors ==="
strings "$LOG" | grep -i "Insufficient\|untyped\|Retype" | head -5
echo ""
echo "=== IPC messages ==="
strings "$LOG" | grep "\[ipc\]" | head -25
echo ""
echo "=== IPC RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== PCI devices found ==="
strings "$LOG" | grep "Found crossvm\|vendor=0x1af4\|vendor=0x" | head -10
echo ""
echo "=== cross_vm init ==="
strings "$LOG" | grep -i "cross_vm\|camkes_cross\|connection.*init" | head -5
echo ""
echo "=== VMM boot ==="
strings "$LOG" | grep -i "Booting all\|dropped to user\|Finalising\|mutex" | head -10
