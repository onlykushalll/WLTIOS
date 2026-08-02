#!/bin/bash
LOG=/home/kushal/torpd-c1-process-listing.log
echo "=== VM0 PROCESS LISTING ==="
sed -n '/VM0-BOOT-INIT-RAN/,/VM0-PS-DONE/p' "$LOG" 2>/dev/null | strings
echo ""
echo "=== VM1 PROCESS LISTING ==="
sed -n '/VM1-BOOT-INIT-RAN/,/VM1-PS-DONE/p' "$LOG" 2>/dev/null | strings
echo ""
echo "=== VM1 TOR LOG ==="
sed -n '/VM1-TOR-LOG-DONE/p' "$LOG" 2>/dev/null | strings
echo ""
echo "=== TOR-PD MESSAGES ==="
strings "$LOG" | grep -i "tor-pd\|tor present\|Starting Tor" | head -10
echo ""
echo "=== INIT RAN? ==="
strings "$LOG" | grep "BOOT-INIT-RAN" | head -5
echo ""
echo "=== KERNEL PANIC? ==="
strings "$LOG" | grep -i "panic\|No working init\|execve failed\|Exec format" | head -5
echo ""
echo "=== RAMDISK SIZES (per-VM isolation proof) ==="
strings "$LOG" | grep "RAMDISK" | head -5
echo ""
echo "=== LAST 20 LINES ==="
strings "$LOG" | tail -20
