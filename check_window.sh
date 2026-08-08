#!/bin/bash
LOG=/home/kushal/torpd-torfix-window.log
echo "=== PCI messages ==="
strings "$LOG" | grep "pci 0000\|can't claim\|BAR\|PCI Root\|PCI host" | head -15
echo ""
echo "=== Bridge ==="
strings "$LOG" | grep "\[bridge\]" | head -15
echo ""
echo "=== RESULT ==="
strings "$LOG" | grep "RESULT:" | head -5
echo ""
echo "=== Tor ==="
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo ""
echo "=== Tor log ==="
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
echo ""
echo "=== segfault ==="
strings "$LOG" | grep "segfault" | head -3
echo ""
echo "=== VMs ==="
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo ""
echo "=== wlt-shell ==="
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
echo ""
echo "=== Last 20 ==="
strings "$LOG" | tail -20
