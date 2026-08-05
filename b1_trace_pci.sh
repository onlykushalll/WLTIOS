#!/bin/bash
# B.1: Trace how the VMM registers PCI config space I/O port handlers (0xCF8/0xCFC).
echo "=== 1. Search for PCI config space I/O port registration ==="
grep -rn "0xCF8\|0xCFC\|0xcf8\|0xcfc\|PCI_CONFIG\|pci_config\|ioport.*pci\|pci.*ioport" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/ \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/ \
    2>/dev/null | head -20

echo ""
echo "=== 2. Search for PCI host bridge setup ==="
grep -rn "host_bridge\|pci_host\|pci_bus\|pci_scan\|pci_register\|vmm_pci" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/main.c \
    2>/dev/null | head -20

echo ""
echo "=== 3. Read main.c around PCI initialization ==="
grep -n "pci\|PCI" /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -30

echo ""
echo "=== 4. Check how vmm_pci_space is created ==="
grep -rn "vmm_pci_space\|pci_space\|vmm_pci_new\|vmm_pci_init" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/main.c \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/ \
    2>/dev/null | head -20

echo ""
echo "=== 5. Check how PCI I/O ports are registered with the guest ==="
grep -rn "ioport_register\|register_ioport\|io_port\|IOPort\|iospace" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/main.c \
    2>/dev/null | head -20

echo ""
echo "=== 6. Check the PCIConfig component ==="
cat /home/kushal/wltios-build/projects/vm/components/PCIConfig/PCIConfig.camkes 2>/dev/null | head -20
grep -rn "pci_config\|PCIConfig" /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -10

echo ""
echo "=== 7. Check how the guest sees PCI (ACPI _SEG, _BBN, MCFG) ==="
# The guest needs ACPI MCFG table or PCI host bridge device to find PCI devices
grep -rn "MCFG\|mcfg\|_SEG\|_BBN\|host_bridge\|pci_host" \
    /home/kushal/wltios-build/projects/vm/components/ \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/ \
    2>/dev/null | head -15

echo ""
echo "=== 8. Check if there's a PCI I/O port fault handler ==="
grep -rn "0xcf8\|0xcfc\|pci_io\|config_space\|pci_config_read\|pci_config_write" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/ \
    2>/dev/null | head -15
