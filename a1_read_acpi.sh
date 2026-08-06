#!/bin/bash
# A.1: Read the VMM's ACPI table generation code to check for PCI host bridge.
echo "=== 1. Read acpi.c (VMM ACPI table generation) ==="
cat /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c 2>/dev/null | head -200

echo ""
echo "=== 2. Check for DSDT generation ==="
grep -rn "DSDT\|dsdt\|_SB\|PCI0\|_SEG\|_BBN\|_CRS\|acpi_dsdt\|AML" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/ \
    2>/dev/null | head -20

echo ""
echo "=== 3. Check the guest_boot_init.c for ACPI setup ==="
grep -rn "acpi\|ACPI\|make_guest" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/guest_boot_init.c \
    2>/dev/null | head -20

echo ""
echo "=== 4. Check what ACPI tables the VMM creates ==="
grep -n "tables\[" /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c 2>/dev/null | head -10

echo ""
echo "=== 5. Check the list of ACPI tables added ==="
sed -n '143,250p' /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c 2>/dev/null
