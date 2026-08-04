#!/bin/bash
LOG=/home/kushal/torpd-c2-probeall-final.log

echo "=== How many Linux versions booted? ==="
strings "$LOG" | grep -c "Linux version"

echo ""
echo "=== How many RAMDISK lines? ==="
strings "$LOG" | grep -c "RAMDISK"

echo ""
echo "=== RAMDISK sizes ==="
strings "$LOG" | grep "RAMDISK" | head -5

echo ""
echo "=== How many 'Booting all finished'? ==="
strings "$LOG" | grep -c "Booting all finished"

echo ""
echo "=== How many 'Initializing guest to start'? ==="
strings "$LOG" | grep -c "Initializing guest to start"

echo ""
echo "=== How many WLTIOS-BOOT markers? ==="
strings "$LOG" | grep -c "WLTIOS-BOOT"

echo ""
echo "=== How many wlt-shell starting? ==="
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -10

echo ""
echo "=== seL4 errors ==="
strings "$LOG" | grep "seL4\|Insufficient\|Untyped\|Retype" | head -5

echo ""
echo "=== Full boot (first 60 lines) ==="
strings "$LOG" | head -60
