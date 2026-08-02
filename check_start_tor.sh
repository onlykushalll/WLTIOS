#!/bin/bash
LOG=/home/kushal/torpd-c1-start-tor.log

echo "=== tor-pd messages ==="
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -10

echo ""
echo "=== tor in PID lines ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10

echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -10

echo ""
echo "=== tor log ==="
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20

echo ""
echo "=== ALL unique PIDs ==="
strings "$LOG" | grep "^PID " | sort -u | head -40

echo ""
echo "=== PS section (first occurrence) ==="
strings "$LOG" | grep -A30 "WLTIOS-PS-BEGIN" | head -35
