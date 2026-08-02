#!/bin/bash
LOG=/home/kushal/torpd-c1-process-listing.log

echo "=== WLTIOS-INIT-RAN markers ==="
strings "$LOG" | grep "WLTIOS-INIT-RAN" | head -5

echo ""
echo "=== WLTIOS-PS process listing ==="
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" 2>/dev/null | strings | head -30

echo ""
echo "=== wlt-check messages ==="
strings "$LOG" | grep "wlt-check\|tor present\|tor log\|tor-pd\|Starting Tor" | head -15

echo ""
echo "=== WLTIOS-INIT-DONE ==="
strings "$LOG" | grep "WLTIOS-INIT-DONE" | head -5

echo ""
echo "=== Kernel cmdline ==="
strings "$LOG" | grep "Kernel command line" | head -2

echo ""
echo "=== RAMDISK sizes ==="
strings "$LOG" | grep "RAMDISK" | head -4

echo ""
echo "=== binfmt/execve errors ==="
strings "$LOG" | grep "execve\|binfmt\|Exec format\|No working init" | head -5

echo ""
echo "=== LAST 20 LINES ==="
strings "$LOG" | tail -20
