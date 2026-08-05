#!/bin/bash
# B.1: The PCI handler is registered at line 879, cross_vm at line 912.
# The cross_vm connections add PCI devices to the vmm_pci_space.
# The guest kernel boots AFTER both, so the order should be fine.
# The real issue: the guest's PCI scan reads 0xFFFFFFFF because the
# vmm_pci_io_port_in function doesn't find the device in the pci space.
#
# Let's add VMM debug logging to see if the PCI config reads are happening.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=/home/kushal/wltios-build

echo "=== 1. Read vmm_pci_io_port_in (the handler) ==="
grep -rn "vmm_pci_io_port_in\|vmm_pci_io_port_out" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/ \
    2>/dev/null | head -10

echo ""
echo "=== 2. Read the pci.c source (where vmm_pci_io_port_in is defined) ==="
cat /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/drivers/pci.c 2>/dev/null | head -80

echo ""
echo "=== 3. Check the vmm_pci_space structure ==="
grep -rn "vmm_pci_space_t\|bus0\|pci_space" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/include/ \
    2>/dev/null | head -15

echo ""
echo "=== 4. Check how vmm_pci_add_entry stores the device ==="
grep -rn "vmm_pci_add_entry\|pci_space->bus0" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/drivers/pci.c \
    2>/dev/null | head -10

echo ""
echo "=== 5. Check what bus/dev the cross_vm connections use ==="
grep -rn "bus\|dev\|fun\|address\|0x1af4\|vendor_id\|device_id" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/drivers/cross_vm_connection.c \
    2>/dev/null | head -20

echo ""
echo "=== 6. Check if the guest's PCI scan reads bus 0 dev 0 ==="
# Linux PCI scan reads bus 0, dev 0, fun 0, offset 0 (vendor ID)
# If the VMM's vmm_pci_space has a device at bus 0 dev 0, it should respond
LOG=/home/kushal/torpd-c2-128mb.log
strings "$LOG" | grep -i "PCI.*scan\|pci.*bus\|pci.*device\|0xcf8\|0xcfc" | head -10
