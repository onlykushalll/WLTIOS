#!/bin/bash
LOG=/home/kushal/torpd-c3-tunables.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== tor-pd messages ==="
strings "$LOG" | grep "tor-pd\|Starting Tor\|GLIBC_TUNABLES\|ISA check" | head -5
echo ""
echo "=== tor PIDs ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -10
echo ""
echo "=== wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -5
echo ""
echo "=== tor log ==="
strings "$LOG" | grep -A30 "WLTIOS-TOR-LOG" | head -35
echo ""
echo "=== CPU ISA errors ==="
strings "$LOG" | grep "CPU ISA\|ISA level" | head -3
echo ""
echo "=== Boot summary ==="
echo "panic: $(grep -ci 'kernel panic' "$LOG" 2>/dev/null)"
