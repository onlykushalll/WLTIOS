#!/bin/bash
# Deep research part 2: How the guest accesses the shared memory dataport.
echo "=== 1. Read existing vm_ipc_test.c ==="
cat /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/src/userspace/wlt-ipc-test/vm_ipc_test.c 2>/dev/null

echo ""
echo "=== 2. Read the crossvm driver source ==="
find /home/kushal/wltios-build/projects -name "crossvm.c" -o -name "cross_vm_connection.c" 2>/dev/null | head -5
cat /home/kushal/wltios-build/projects/vm/components/Init/src/crossvm.c 2>/dev/null | head -80

echo ""
echo "=== 3. Read the cross_vm_connection driver ==="
find /home/kushal/wltios-build/projects -name "cross_vm_connection*" 2>/dev/null | head -5
cat /home/kushal/wltios-build/projects/seL4_libs/libsel4vmmplatsupport/src/drivers/cross_vm_connection.c 2>/dev/null | head -100

echo ""
echo "=== 4. Check how the dataport is mapped (PCI BAR address) ==="
grep -rn "connection_base_address\|cross_vm_connection\|dataport.*pci\|BAR" /home/kushal/wltios-build/projects/seL4vmmplatsupport/ /home/kushal/wltios-build/projects/seL4_libs/libsel4vmmplatsupport/ 2>/dev/null | head -20

echo ""
echo "=== 5. Check the VM config for connection base address ==="
grep -rn "connection_base\|cross_vm_base\|0x30000000\|dataport_base" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/ /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head -15
