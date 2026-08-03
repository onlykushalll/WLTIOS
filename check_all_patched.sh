#!/bin/bash
LOG=/home/kushal/torpd-c3-all-patched.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== tor-pd ==="
strings "$LOG" | grep "tor-pd\|TOR_EXEC\|EXEC_FAIL" | head -5
echo ""
echo "=== tor PIDs ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor:" | head -5
echo ""
echo "=== tor log ==="
strings "$LOG" | grep -A35 "WLTIOS-TOR-LOG" | head -40
echo ""
echo "=== ISA errors ==="
strings "$LOG" | grep "CPU ISA\|ISA level\|lower than" | head -3
