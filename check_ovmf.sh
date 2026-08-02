#!/bin/bash
cd /tmp/ovmf-test2 2>/dev/null || exit 1
echo "=== LOG SIZE ==="
wc -l ovmf_serial.log 2>/dev/null
wc -c ovmf_serial.log 2>/dev/null
echo ""
echo "=== KEY MESSAGES ==="
strings ovmf_serial.log | grep -i "WLTIOS\|Boot Loader\|ExitBootServices\|STUB\|error\|fail\|BdsDxe\|loading\|Booting\|kernel\|Memory" | head -20
echo ""
echo "=== FIRST 30 NON-EMPTY LINES ==="
strings ovmf_serial.log | head -30
echo ""
echo "=== LAST 20 LINES ==="
strings ovmf_serial.log | tail -20
