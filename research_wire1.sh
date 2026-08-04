#!/bin/bash
# Deep research: Trace exactly how optiplex9020 registers the cross_vm init handler.
echo "=== 1. Read connections.h (VM_CONNECTION_INIT_HANDLER macro) ==="
cat /home/kushal/wltios-build/projects/vm/configurations/connections.h 2>/dev/null | head -80

echo ""
echo "=== 2. Read vm.h (VM_INIT_DEF, VM_PER_VM_COMP_DEF, VM_PER_VM_CONFIG_DEF) ==="
cat /home/kushal/wltios-build/projects/vm/configurations/vm.h 2>/dev/null | head -100

echo ""
echo "=== 3. Read vm.camkes (VM_COMPOSITION_DEF etc) ==="
cat /home/kushal/wltios-build/projects/vm/components/VM/vm.camkes 2>/dev/null | head -60

echo ""
echo "=== 4. Check optiplex9020 CMakeLists ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/CMakeLists.txt 2>/dev/null
