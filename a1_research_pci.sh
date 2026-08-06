#!/bin/bash
# A.1: Deep research — why guest kernel PCI scan finds 0 devices.
# Key clue: raw 0xCF8/0xCFC works (finds 3 devices) but /sys/bus/pci/devices is empty.
# Kernel says "PCI: Using configuration type 1 for base access" (correct — type 1 = 0xCF8/0xCFC)
# Kernel says "PCI: Using host bridge windows from ACPI" — so ACPI must describe the host bridge.
# But no "pci 0000:00:xx.x" enumeration messages appear.
#
# Hypothesis: The VMM's ACPI tables don't include a PCI host bridge device (no _SB.PCI0 node)
# or the MCFG table is missing, so the kernel's PCI subsystem initializes but doesn't scan.

echo "=== 1. Check guest kernel config (is CONFIG_PCI=y?) ==="
# The bzimage is at build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage
# Check if kernel config is embedded
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "CONFIG_PCI\|CONFIG_ACPI\|CONFIG_PCI_HOST" | head -10

echo ""
echo "=== 2. Check if kernel config file exists in the build tree ==="
find /home/kushal/wltios-build -name ".config" -path "*linux*" -o -name "kernel.config" -o -name "config-*" 2>/dev/null | head -10

echo ""
echo "=== 3. Check the camkes-vm-linux kernel config ==="
find /home/kushal/wltios-build/projects/vm-linux -name "*.config" -o -name "defconfig" 2>/dev/null | head -10
find /home/kushal/wltios-build/projects/vm-linux -name "x86_64*config*" -o -name "pc99*config*" 2>/dev/null | head -10

echo ""
echo "=== 4. Check the VMM's ACPI table generation ==="
# The VMM creates ACPI tables for the guest. Check if it includes a PCI host bridge.
grep -rn "MCFG\|mcfg\|PCI0\|_SB.*PCI\|host_bridge\|pci_host\|ACPI.*PCI" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/ \
    /home/kushal/wltios-build/projects/vm/components/Init/src/ \
    2>/dev/null | head -20

echo ""
echo "=== 5. Check the VMM's ACPI table source ==="
find /home/kushal/wltios-build/projects -name "acpi*" -name "*.c" -o -name "acpi*" -name "*.h" 2>/dev/null | head -10
grep -rn "acpi_table\|ACPI_TABLE\|rsdt\|RSDT\|fadt\|FADT\|mcfg\|MCFG" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/ \
    2>/dev/null | head -15

echo ""
echo "=== 6. Check if the VMM generates an MCFG table ==="
grep -rn "MCFG\|mcfg\|pci_mcfg\|acpi_mcfg" \
    /home/kushal/wltios-build/projects/ \
    2>/dev/null | grep -v ".o:" | head -15

echo ""
echo "=== 7. Check boot log for ACPI PCI host bridge messages ==="
LOG=/home/kushal/torpd-c2-128mb.log
strings "$LOG" | grep -i "ACPI.*PCI\|PCI.*ACPI\|host.*bridge\|_SB\|PCI0\|MCFG\|mcfg" | head -10

echo ""
echo "=== 8. Check if the kernel has CONFIG_PCI_MMCONFIG ==="
# If the kernel expects MMCONFIG but the VMM only provides type 1 I/O,
# the scan might not work
strings /home/kushal/wltios-build/build-tor-pd-vm1/apps/x86/tor_pd_vm1/fserv/files/bzimage | grep -i "mmconfig\|MMCONFIG\|ecam\|ECAM" | head -5
