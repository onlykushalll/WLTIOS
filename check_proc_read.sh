#!/bin/bash
LOG=/home/kushal/torpd-c1-proc-read.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PS LISTING (all occurrences) ==="
# Use grep to find all PS-BEGIN/END blocks (output is interleaved)
strings "$LOG" | grep -A20 "WLTIOS-PS-BEGIN" | head -60
echo ""
echo "=== PID lines ==="
strings "$LOG" | grep "^PID " | head -20
echo ""
echo "=== WLT-CHECK ==="
strings "$LOG" | grep "wlt-check\|tor processes\|tor-pd\|tor present" | head -10
echo ""
echo "=== TOR LOG ==="
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
echo ""
echo "=== LAST 15 LINES ==="
strings "$LOG" | tail -15
