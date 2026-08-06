#!/bin/bash
# A.1+A.2: Create a DSDT with PCI host bridge + embed in VMM ACPI generation.
set -e

echo "=== 1. Check if iasl (ACPI compiler) is available ==="
which iasl 2>/dev/null || echo "iasl not found"
which acpica-tools 2>/dev/null || echo "acpica-tools not found"

echo ""
echo "=== 2. Install iasl ==="
sudo apt-get install -y acpica-tools 2>&1 | tail -3
which iasl

echo ""
echo "=== 3. Create ASL source for minimal PCI host bridge DSDT ==="
cat > /tmp/wltios_dsdt.asl << 'ASL'
/*
 * WLTIOS minimal DSDT with PCI host bridge.
 * This tells the Linux kernel there's a PCI bus at bus 0,
 * triggering the PCI bus scan that discovers crossvm devices.
 */
DefinitionBlock ("wltios_dsdt.aml", "DSDT", 2, "WLTIOS", "WLTIOS1", 1)
{
    Scope (\_SB)
    {
        Device (PCI0)
        {
            /* PCI Host Bridge */
            Name (_HID, EisaId ("PNP0A03"))
            Name (_BBN, Zero)   /* Bus number 0 */
            Name (_SEG, Zero)   /* Segment 0 */

            Method (_CRS, 0, NotSerialized)
            {
                /* Current Resource Settings: PCI config I/O + memory window */
                Name (BUF0, ResourceTemplate ()
                {
                    /* PCI Config Space I/O ports 0xCF8-0xCFF */
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                            0x0000,         /* Granularity */
                            0x0CF8,         /* Min */
                            0x0CFF,         /* Max */
                            0x0000,         /* Translation */
                            0x0008,         /* Length */
                            , , TypeStatic, DenseTranslation)
                    /* PCI Memory Window: 0xC0000000-0xDFFFFFFF (512MB) */
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                            0x00000000,     /* Granularity */
                            0xC0000000,     /* Min */
                            0xDFFFFFFF,     /* Max */
                            0x00000000,     /* Translation */
                            0x20000000,     /* Length */
                            , , , , TypeStatic)
                    /* PCI I/O Port Window: 0xC000-0xCFFF */
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                            0x0000,
                            0xC000,
                            0xCFFF,
                            0x0000,
                            0x1000,
                            , , TypeStatic, DenseTranslation)
                })
                Return (BUF0)
            }

            /* PCI IRQ routing — route all to IRQ 11 (legacy) */
            Method (_PRT, 0, NotSerialized)
            {
                Return (Package (0x01)
                {
                    Package (0x04) { 0x0018FFFF, 0x00, Zero, 0x0B }  /* INTA = IRQ 11 */
                })
            }
        }
    }
}
ASL

echo "ASL source created"

echo ""
echo "=== 4. Compile ASL to AML ==="
iasl /tmp/wltios_dsdt.asl 2>&1
echo "compile exit: $?"
ls -la /tmp/wltios_dsdt.aml 2>/dev/null

echo ""
echo "=== 5. Convert AML to C header ==="
xxd -i /tmp/wltios_dsdt.aml > /tmp/wltios_dsdt_aml.h 2>/dev/null
echo "--- AML header ---"
head -5 /tmp/wltios_dsdt_aml.h
echo "..."
wc -l /tmp/wltios_dsdt_aml.h

echo ""
echo "=== 6. Create the patch for acpi.c ==="
# The patch replaces the empty DSDT with one containing the PCI host bridge AML
ACPIC=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c
echo "--- Current DSDT creation code ---"
grep -n "DSDT\|dsdt" "$ACPIC" | head -10

echo ""
echo "=== 7. Read the DSDT section of acpi.c ==="
sed -n '195,220p' "$ACPIC" 2>/dev/null
