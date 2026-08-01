#!/bin/bash
echo "=== Search bzimage for WLTIOS strings ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "WLTIOS\|wlt-init\|WLT-Shell\|WLT-Init" | head -10
echo ""
echo "=== Search vm0.instance.bin ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep -i "WLTIOS\|wlt-init\|WLT-Shell\|WLT-Init" | head -10
echo ""
echo "=== Search vm1.instance.bin ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm1.instance.bin 2>/dev/null | grep -i "WLTIOS\|wlt-init\|WLT-Shell\|WLT-Init" | head -10
echo ""
echo "=== Search capdl-loader ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/capdl-loader 2>/dev/null | grep -i "WLTIOS\|wlt-init\|WLT-Shell\|WLT-Init" | head -10
echo ""
echo "=== Search the entire build tree ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "*.bin" -o -name "*.elf" -o -name "capdl*" | while read f; do
  matches=$(strings "$f" 2>/dev/null | grep -c "WLT-Shell\|WLTIOS Boot\|wlt-init")
  if [ "$matches" -gt 0 ]; then
    echo "$f: $matches matches"
    strings "$f" | grep -i "WLTIOS\|wlt-init\|WLT-Shell" | head -3
  fi
done
