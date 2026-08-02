#!/bin/bash
LOG=/home/kushal/torpd-c1-final-libs.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== tor-pd messages ==="
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo ""
echo "=== tor in PID lines ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -10
echo ""
echo "=== tor log ==="
strings "$LOG" | grep -A20 "WLTIOS-TOR-LOG" | head -25
echo ""
echo "=== ALL unique PIDs ==="
strings "$LOG" | grep "^PID " | sort -u | head -40
