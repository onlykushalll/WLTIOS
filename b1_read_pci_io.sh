#!/bin/bash
# B.1: Read the full PCI I/O port registration section + fix the commented-out handler.
echo "=== 1. Read main.c lines 320-345 (I/O port registration) ==="
sed -n '320,345p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 2. Read the vmm_pci_io_port functions ==="
grep -n "vmm_pci_io_port_in\|vmm_pci_io_port_out\|X86_IO_PCI_CONFIG" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/main.c \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/include/ \
    2>/dev/null | head -15

echo ""
echo "=== 3. Find X86_IO_PCI_CONFIG_START/END ==="
grep -rn "X86_IO_PCI_CONFIG_START\|X86_IO_PCI_CONFIG_END\|PCI_CONFIG_START\|PCI_CONFIG_END" \
    /home/kushal/wltios-build/projects/ \
    2>/dev/null | head -10

echo ""
echo "=== 4. Check how optiplex9020 handles PCI config I/O ==="
grep -n "PCI_CONFIG\|0xCF8\|0xCFC\|pci_config_io\|vmm_pci_io_port" \
    /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/ \
    2>/dev/null | head -10

echo ""
echo "=== 5. Check the pci_config_io_in/out functions ==="
sed -n '340,425p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 6. Check how the I/O port list is built + if PCI config is registered ==="
sed -n '620,660p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null
