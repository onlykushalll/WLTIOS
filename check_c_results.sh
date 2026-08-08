#!/bin/bash
# Check Section C boot test results from last session.
echo "=== 1. Check if bridge test log exists ==="
LOG=/home/kushal/torpd-c-bridge.log
ls -la "$LOG" 2>/dev/null
wc -c "$LOG" 2>/dev/null

echo ""
echo "=== 2. Bridge messages ==="
strings "$LOG" | grep "\[bridge\]" | head -25

echo ""
echo "=== 3. Bridge RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5

echo ""
echo "=== 4. Tor PIDs ==="
strings "$LOG" | grep "^PID " | grep -i "tor" | head -5

echo ""
echo "=== 5. wlt-check ==="
strings "$LOG" | grep "wlt-check\|tor processes" | head -5

echo ""
echo "=== 6. Tor log ==="
strings "$LOG" | grep -A5 "WLTIOS-TOR-LOG" | head -10

echo ""
echo "=== 7. VMs booted ==="
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "RAMDISK: $(strings "$LOG" | grep -c 'RAMDISK')"

echo ""
echo "=== 8. wlt-shell starting ==="
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5

echo ""
echo "=== 9. PCI messages ==="
strings "$LOG" | grep "PCI Root Bridge\|PCI host bridge\|pci_bus" | head -5

echo ""
echo "=== 10. Last 20 lines ==="
strings "$LOG" | tail -20
