#!/bin/bash
LOG=/home/kushal/torpd-c2-dp-config2.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== IPC messages ==="
strings "$LOG" | grep "\[ipc\]" | head -30
echo ""
echo "=== IPC RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== PCI devices found ==="
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo ""
echo "=== WLTIOS-BOOT ==="
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell.*starting\|IPC sender\|IPC receiver" | head -10
echo ""
echo "=== Boot errors ==="
strings "$LOG" | grep -i "error\|fail\|panic" | grep -v "^\[" | head -5
