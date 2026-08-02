#!/bin/bash
cd /tmp/ovmf-test3 2>/dev/null || { echo "DIR NOT FOUND"; exit 1; }
echo "=== FILES ==="
ls -la disk.img fat.img ovmf_serial.log 2>/dev/null
echo ""
echo "=== GPT CHECK ==="
sgdisk -p disk.img 2>/dev/null || parted -s disk.img print 2>/dev/null || echo "No partition tool"
echo ""
echo "=== SERIAL LOG ==="
wc -c ovmf_serial.log 2>/dev/null
strings ovmf_serial.log 2>/dev/null | grep -i "WLTIOS\|Boot\|ExitBoot\|STUB\|error\|fail\|BdsDxe\|loading\|kernel" | head -20
echo ""
echo "=== ALL NON-EMPTY ==="
strings ovmf_serial.log 2>/dev/null | grep -v "^\[" | grep -v "^$" | head -25
