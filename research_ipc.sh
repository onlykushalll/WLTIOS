#!/bin/bash
# Deep research: How camkes seL4SharedDataWithCaps dataports work for VM-to-VM IPC.
echo "=== 1. Read the camkes assembly (minimal_tor_pd.camkes) ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>/dev/null

echo ""
echo "=== 2. Read cross_vm_connections.c (vm0 side) ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/cross_vm_connections.c 2>/dev/null

echo ""
echo "=== 3. Read cross_vm_connections_vm1.c (vm1 side) ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/cross_vm_connections_vm1.c 2>/dev/null

echo ""
echo "=== 4. Check for existing vm_ipc_test tool ==="
find /home/kushal/wltios-build -name "vm_ipc_test*" 2>/dev/null | head -5
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS -name "vm_ipc_test*" 2>/dev/null | head -5

echo ""
echo "=== 5. Search for how dataports appear to the guest ==="
# The camkes VM framework maps dataports as PCI BARs or /dev/mem regions.
# Check the VM component source for how it exposes shared memory.
grep -rn "dataport\|shared.*memory\|cross_vm\|vm_shm\|SHM" /home/kushal/wltios-build/projects/vm/components/Init/src/ 2>/dev/null | head -20

echo ""
echo "=== 6. Check for camkes dataport header generation ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "*tor_src*" -o -name "*tor_dest*" -o -name "*dataport*" 2>/dev/null | head -10

echo ""
echo "=== 7. Check the generated camkes component headers ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "camkes-component-*.h" 2>/dev/null | head -5

echo ""
echo "=== 8. Look at the Init component's camkes template ==="
cat /home/kushal/wltios-build/projects/vm/components/Init/Init.camkes 2>/dev/null | head -30
