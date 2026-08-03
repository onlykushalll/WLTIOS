#!/bin/bash
LOG=/home/kushal/torpd-c2-ipc-fix.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== IPC messages ==="
strings "$LOG" | grep "\[ipc\]" | head -25
echo ""
echo "=== IPC RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== WLTIOS-BOOT markers ==="
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell.*starting\|IPC sender\|IPC receiver" | head -10
echo ""
echo "=== tor PIDs ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -5
echo ""
echo "=== ISA errors ==="
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo ""
echo "=== ALL unique PIDs ==="
strings "$LOG" | grep "^PID " | sort -u | head -30
