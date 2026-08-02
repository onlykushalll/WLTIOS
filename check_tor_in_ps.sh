#!/bin/bash
LOG=/home/kushal/torpd-c1-proc-read.log

echo "=== ALL PID lines (sorted) ==="
strings "$LOG" | grep "^PID " | sort -t' ' -k2 -n | uniq | head -40

echo ""
echo "=== Search for 'tor' in PID lines ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10

echo ""
echo "=== WLT-CHECK messages ==="
strings "$LOG" | grep "wlt-check\|tor processes\|tor present" | head -10

echo ""
echo "=== TOR-LOG section ==="
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15

echo ""
echo "=== ALL unique PID:comm pairs ==="
strings "$LOG" | grep "^PID " | sort -u | head -40
