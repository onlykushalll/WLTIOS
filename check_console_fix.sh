#!/bin/bash
LOG=/home/kushal/torpd-c1-console-fix.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PS LISTING (VM0 + VM1) ==="
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -30
echo ""
echo "=== WLT-CHECK ==="
strings "$LOG" | grep "wlt-check\|tor present\|tor-pd\|Starting Tor" | head -10
echo ""
echo "=== TOR LOG ==="
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
echo ""
echo "=== RAMDISK ==="
strings "$LOG" | grep "RAMDISK" | head -4
echo ""
echo "=== EXECVE ERRORS ==="
strings "$LOG" | grep "execve\|Failed to execute" | head -5
echo ""
echo "=== LAST 20 LINES ==="
strings "$LOG" | tail -20
