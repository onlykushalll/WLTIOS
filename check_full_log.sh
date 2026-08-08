#!/bin/bash
LOG=/home/kushal/torpd-tor-first.log
echo "=== Full log search ==="
echo "--- bridge/wlt-shell/tor (case insensitive) ---"
strings "$LOG" | grep -i "bridge\|wlt-shell\|tor-pd\|WLTIOS-BOOT\|RESULT" | head -20
echo ""
echo "--- All messages after timestamp 10.0 ---"
strings "$LOG" | grep "^\[ *1[0-9]\." | head -20
echo ""
echo "--- QEMU termination ---"
strings "$LOG" | grep "terminating\|signal\|EXIT" | head -3
echo ""
echo "--- Log size ---"
wc -c "$LOG"
wc -l "$LOG"
