#!/bin/bash
# Find vm.h and connections.h (the config macros).
echo "=== Find vm.h ==="
find /home/kushal/wltios-build/projects -name "vm.h" -path "*/configurations/*" 2>/dev/null | head -5
find /home/kushal/wltios-build -name "vm.h" -path "*/configurations/*" 2>/dev/null | head -5

echo ""
echo "=== Find connections.h ==="
find /home/kushal/wltios-build/projects -name "connections.h" 2>/dev/null | head -5

echo ""
echo "=== Find VM_INIT_DEF ==="
grep -rn "VM_INIT_DEF\|VM_PER_VM_COMP_DEF\|VM_COMPOSITION_DEF" /home/kushal/wltios-build/projects/vm/ 2>/dev/null | grep "#define" | head -10

echo ""
echo "=== Find VM_CONNECTION_INIT_HANDLER ==="
grep -rn "VM_CONNECTION_INIT_HANDLER\|CONNECTION_INIT" /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head -10

echo ""
echo "=== Read the Init.camkes (component definition) ==="
cat /home/kushal/wltios-build/projects/vm/components/Init/Init.camkes 2>/dev/null

echo ""
echo "=== Check how cross_vm_connections_init is called ==="
grep -rn "camkes_cross_vm_connections_init\|cross_vm_connections_init" /home/kushal/wltios-build/projects/vm/components/Init/src/ 2>/dev/null | head -10

echo ""
echo "=== Read main.c around cross_vm init ==="
grep -n "cross_vm\|connection_init\|camkes_cross" /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -15
