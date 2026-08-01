#!/bin/bash
echo "=== VMM SOURCE: WLTIOS/wlt-init ==="
grep -rn "WLTIOS Boot" /home/kushal/wltios-build/projects/vm/components/Init/src/ /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/ 2>/dev/null | head -10
echo ""
grep -rn "wlt-init" /home/kushal/wltios-build/projects/vm/components/Init/src/ /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/ 2>/dev/null | head -10
echo ""
grep -rn "WLT-Shell" /home/kushal/wltios-build/projects/vm/components/Init/src/ /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/ 2>/dev/null | head -10
echo ""
echo "=== ALL .c FILES in VMM ==="
find /home/kushal/wltios-build/projects/vm/components/Init/src/ -name "*.c" 2>/dev/null | head
echo ""
echo "=== main.c grep ==="
grep -n "WLTIOS\|wlt.init\|WLT.Shell\|WLT.Init" /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -15
