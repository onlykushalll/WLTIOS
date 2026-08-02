#!/bin/bash
LOG=/home/kushal/torpd-c1-binary-wltshell.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PS LISTING ==="
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -30
echo ""
echo "=== WLT-CHECK ==="
strings "$LOG" | grep "wlt-check\|tor present\|tor-pd\|Starting Tor\|Dropping" | head -10
echo ""
echo "=== TOR LOG ==="
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
echo ""
echo "=== LAST 15 LINES ==="
strings "$LOG" | tail -15
