#!/bin/bash
LOG=/home/kushal/torpd-tor-first.log
echo "=== Bridge ==="
strings "$LOG" | grep "\[bridge\]" | head -20
echo "=== RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo "=== Tor ==="
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo "=== Tor log ==="
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "=== segfault ==="
strings "$LOG" | grep "segfault" | head -3
echo "=== VMs ==="
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "=== wlt-shell ==="
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
echo "=== Last 20 ==="
strings "$LOG" | tail -20
