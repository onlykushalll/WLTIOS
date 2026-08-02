#!/bin/bash
LOG=/home/kushal/torpd-c1-process-listing.log

echo "=== Search for /init execution messages ==="
strings "$LOG" | grep -i "run.*init\|/init\|init process\|trying to\|execute /init" | head -10

echo ""
echo "=== Search for 'Run /init' kernel message ==="
strings "$LOG" | grep "Run /" | head -5

echo ""
echo "=== Unpacking initramfs messages ==="
strings "$LOG" | grep -i "Unpacking\|initramfs\|Freeing initrd" | head -10

echo ""
echo "=== All kernel messages from 2.0s to 3.0s (around init time) ==="
strings "$LOG" | grep "^\[    2\.\|^\[    3\." | head -30

echo ""
echo "=== Check for 'No working init' ==="
strings "$LOG" | grep -i "No working init\|kernel_init\|Kernel panic" | head -5

echo ""
echo "=== Check actual cmdline in log ==="
strings "$LOG" | grep "Kernel command line" | head -2

echo ""
echo "=== Check for binfmt-464c (32-bit exec attempt) ==="
strings "$LOG" | grep "binfmt" | head -5

echo ""
echo "=== FULL LOG: lines 200-250 (around init area) ==="
sed -n '200,250p' "$LOG" | strings | head -40
