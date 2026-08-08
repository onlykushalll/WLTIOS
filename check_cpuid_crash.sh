#!/bin/bash
LOG=/home/kushal/torpd-cpuid-passthrough.log
echo "=== Full log ==="
strings "$LOG" | head -30
echo ""
echo "=== Last 10 ==="
strings "$LOG" | tail -10
echo ""
echo "=== Errors ==="
strings "$LOG" | grep -i "error\|fail\|panic\|crash\|assert\|fault" | head -10
