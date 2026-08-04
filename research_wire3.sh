#!/bin/bash
# Read main.c around the cross_vm init call + check CROSS_VM_BASE_ADDRESS.
echo "=== main.c lines 900-930 (cross_vm init call) ==="
sed -n '900,930p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== main.c lines 555-575 (irq check) ==="
sed -n '555,575p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== main.c lines 90-100 (weak declaration) ==="
sed -n '90,100p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== Find CROSS_VM_BASE_ADDRESS ==="
grep -rn "CROSS_VM_BASE_ADDRESS\|cross_vm_base" /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head -10

echo ""
echo "=== Read vm.h (VM_INIT_DEF + VM_PER_VM_COMP_DEF) ==="
sed -n '30,70p' /home/kushal/wltios-build/projects/vm/components/VM/configurations/vm.h 2>/dev/null

echo ""
echo "=== Read vm.h lines 110-140 (composition + per-vm) ==="
sed -n '110,140p' /home/kushal/wltios-build/projects/vm/components/VM/configurations/vm.h 2>/dev/null

echo ""
echo "=== Check if camkes_cross_vm_connections_init is in the built vm0.instance.bin ==="
nm /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep "cross_vm_connections_init" | head -5
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep "cross_vm\|camkes_cross" | head -5
