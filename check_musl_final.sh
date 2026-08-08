#!/bin/bash
LOG=/home/kushal/torpd-musl-final.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== Full search for bridge/tor/wlt ==="
strings "$LOG" | grep -i "bridge\|tor\|wlt-shell\|WLTIOS\|RESULT\|segfault\|ISA\|error\|fail\|shared lib" | head -30
echo ""
echo "=== Last 30 lines ==="
strings "$LOG" | tail -30
