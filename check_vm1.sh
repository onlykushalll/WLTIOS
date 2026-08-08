#!/bin/bash
LOG=/home/kushal/torpd-c3-bridge-fix.log
echo "=== vm1 messages ==="
strings "$LOG" | grep "vm1\|bridge.*vm1\|CONNECT.*trying\|connect.*failed\|bridge.*receiver" | head -20

echo ""
echo "=== ALL bridge messages (full) ==="
strings "$LOG" | grep "\[bridge\]" | head -40

echo ""
echo "=== wlt-shell starting (all) ==="
strings "$LOG" | grep "wlt-shell.*starting" | head -10

echo ""
echo "=== RAMDISK ==="
strings "$LOG" | grep "RAMDISK" | head -5

echo ""
echo "=== ALL unique PIDs ==="
strings "$LOG" | grep "^PID " | sort -u | head -30

echo ""
echo "=== Tor log ==="
strings "$LOG" | grep -A5 "TOR-LOG" | head -10

echo ""
echo "=== segfault ==="
strings "$LOG" | grep "segfault\|tor\[" | head -5
