#!/bin/bash
LOG=/home/kushal/torpd-c-bridge.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== Bridge messages ==="
strings "$LOG" | grep "\[bridge\]" | head -25
echo ""
echo "=== Bridge RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== Tor PID ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -5
echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo ""
echo "=== Tor log ==="
strings "$LOG" | grep -A5 "WLTIOS-TOR-LOG" | head -10
echo ""
echo "=== VMs booted ==="
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo ""
echo "=== wlt-shell starting ==="
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5
echo ""
echo "=== ALL unique PIDs (tor check) ==="
strings "$LOG" | grep "^PID " | sort -u | head -30
