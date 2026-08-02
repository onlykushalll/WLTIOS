#!/bin/bash
cd /tmp/ovmf-test4 2>/dev/null || exit 1
echo "=== LOG SIZE ==="
wc -c ovmf_serial.log
echo ""
echo "=== KEY MESSAGES ==="
strings ovmf_serial.log | grep -i "WLTIOS" | head -5
strings ovmf_serial.log | grep -i "Boot Loader" | head -5
strings ovmf_serial.log | grep -i "ExitBoot" | head -5
strings ovmf_serial.log | grep -i "STUB" | head -5
strings ovmf_serial.log | grep -i "BdsDxe" | head -5
strings ovmf_serial.log | grep -i "Not Found" | head -5
strings ovmf_serial.log | grep -i "error" | head -5
strings ovmf_serial.log | grep -i "loaded" | head -5
echo ""
echo "=== ALL NON-EMPTY ==="
strings ovmf_serial.log | grep -v "^\[" | grep -v "^$" | head -30
