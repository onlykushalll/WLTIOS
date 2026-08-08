#!/bin/bash
# Check ALL boot logs from recent sessions for Tor/bridge results.
echo "=== 1. Check v11 boot log (torpd-tor-first.log) ==="
LOG=/home/kushal/torpd-tor-first.log
if [ -f "$LOG" ]; then
    echo "Size: $(wc -c < "$LOG") bytes"
    echo "--- Bridge ---"
    strings "$LOG" | grep "\[bridge\]" | head -15
    echo "--- RESULT ---"
    strings "$LOG" | grep "RESULT:" | head -5
    echo "--- Tor ---"
    strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
    echo "--- Tor log ---"
    strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
    echo "--- segfault ---"
    strings "$LOG" | grep "segfault" | head -3
    echo "--- VMs ---"
    echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
    echo "--- wlt-shell ---"
    strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
else
    echo "NOT FOUND"
fi

echo ""
echo "=== 2. Check realloc boot log (torpd-torfix-realloc.log) ==="
LOG=/home/kushal/torpd-torfix-realloc.log
if [ -f "$LOG" ]; then
    echo "Size: $(wc -c < "$LOG") bytes"
    echo "--- Bridge ---"
    strings "$LOG" | grep "\[bridge\]" | head -15
    echo "--- RESULT ---"
    strings "$LOG" | grep "RESULT:" | head -5
    echo "--- Tor ---"
    strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
    echo "--- segfault ---"
    strings "$LOG" | grep "segfault" | head -3
    echo "--- VMs ---"
    echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
else
    echo "NOT FOUND"
fi

echo ""
echo "=== 3. List all torpd logs ==="
ls -lt /home/kushal/torpd-*.log 2>/dev/null | head -10
