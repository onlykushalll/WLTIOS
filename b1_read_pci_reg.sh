#!/bin/bash
# B.1: Read the dynamic PCI config registration + check if it's actually called.
echo "=== 1. Read main.c lines 870-895 (PCI config registration) ==="
sed -n '870,895p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 2. Read main.c lines 640-660 (pci_io_ops + scan) ==="
sed -n '640,660p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 3. Read main.c lines 785-800 (vmm_pci_init + libpci_scan) ==="
sed -n '785,800p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 4. Check the full main_continued function for PCI setup order ==="
grep -n "vmm_pci_init\|libpci_scan\|pci_config_range\|vm_register_ioport\|camkes_cross_vm" \
    /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -15

echo ""
echo "=== 5. Check if vm_register_ioport is called for PCI config ==="
sed -n '875,890p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 6. Check the boot log for any PCI-related VMM messages ==="
LOG=/home/kushal/torpd-c2-128mb.log
strings "$LOG" | grep -i "pci\|PCI\|ioport\|io_port\|config.*space" | head -15

echo ""
echo "=== 7. Check if guest kernel even tries to scan PCI ==="
strings "$LOG" | grep -i "pci.*scan\|pci.*probe\|pci.*bus\|pci.*device\|lspci\|0000:" | head -10
