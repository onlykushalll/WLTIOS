#!/bin/bash
LOG=/home/kushal/torpd-torfix-xorret2.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== ALL strings (first 60) ==="
strings "$LOG" | head -60
echo ""
echo "=== WLTIOS-BOOT markers ==="
strings "$LOG" | grep "WLTIOS-BOOT\|wlt-shell\|wlt-init\|bridge\|tor" | head -15
echo ""
echo "=== Last 20 lines ==="
strings "$LOG" | tail -20
echo ""
echo "=== execve errors ==="
strings "$LOG" | grep "execve\|exec\|format\|Permission" | head -5
