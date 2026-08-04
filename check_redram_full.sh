#!/bin/bash
LOG=/home/kushal/torpd-c2-reduced-ram.log
echo "=== Full boot sequence ==="
strings "$LOG" | head -50
echo ""
echo "=== Last 30 lines ==="
strings "$LOG" | tail -30
echo ""
echo "=== VM boot status ==="
strings "$LOG" | grep -i "Linux version\|RAMDISK\|Booting\|panic\|Welcome" | head -10
