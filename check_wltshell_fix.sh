#!/bin/bash
LOG=/home/kushal/torpd-c1-wltshell-fix.log
echo "=== LOG SIZE ==="
wc -c "$LOG" 2>/dev/null
echo ""
echo "=== PS LISTING ==="
sed -n '/WLTIOS-PS-BEGIN/,/WLTIOS-PS-END/p' "$LOG" | strings | head -25
echo ""
echo "=== WLT-CHECK ==="
strings "$LOG" | grep "wlt-check\|tor present\|tor-pd\|Starting Tor" | head -10
echo ""
echo "=== TOR LOG ==="
sed -n '/WLTIOS-TOR-LOG/,/WLTIOS-TOR-LOG-END/p' "$LOG" | strings | head -10
echo ""
echo "=== KERNEL CMDLINE ==="
strings "$LOG" | grep "Kernel command line" | head -2
echo ""
echo "=== RAMDISK ==="
strings "$LOG" | grep "RAMDISK" | head -4
echo ""
echo "=== EXECVE ERRORS ==="
strings "$LOG" | grep "execve\|Failed to execute\|No working init\|Run /" | head -10
echo ""
echo "=== LAST 20 LINES ==="
strings "$LOG" | tail -20
