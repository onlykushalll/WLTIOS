#!/bin/bash
LOG=/home/kushal/torpd-c3-chmod-fix.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== tor-pd messages ==="
strings "$LOG" | grep "tor-pd\|Starting Tor" | head -5
echo ""
echo "=== tor PIDs ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo ""
echo "=== tor log ==="
strings "$LOG" | grep -A35 "WLTIOS-TOR-LOG" | head -40
echo ""
echo "=== CPU ISA errors ==="
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo ""
echo "=== EXEC errors ==="
strings "$LOG" | grep "TOR_EXEC\|EXEC_FAILED\|Permission denied" | head -5
