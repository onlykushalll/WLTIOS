#!/bin/bash
# Create ASL → compile to AML → patch VMM acpi.c to embed PCI host bridge DSDT.
set -e
ACPIC=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c

echo "=== 1. Create ASL source ==="
cat > /tmp/wltios_dsdt.asl << 'ASL'
DefinitionBlock ("wltios_dsdt.aml", "DSDT", 2, "WLTIOS", "WLTIOS1", 1)
{
    Scope (\_SB)
    {
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03"))
            Name (_BBN, Zero)
            Name (_SEG, Zero)
            Method (_CRS, 0, NotSerialized)
            {
                Name (BUF0, ResourceTemplate ()
                {
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                            0x0000, 0x0CF8, 0x0CFF, 0x0000, 0x0008, , , TypeStatic, DenseTranslation)
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                            0x00000000, 0xC0000000, 0xDFFFFFFF, 0x00000000, 0x20000000, , , , , TypeStatic)
                    WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                            0x0000, 0xC000, 0xCFFF, 0x0000, 0x1000, , , TypeStatic, DenseTranslation)
                })
                Return (BUF0)
            }
            Method (_PRT, 0, NotSerialized)
            {
                Return (Package (0x01) { Package (0x04) { 0x0018FFFF, 0x00, Zero, 0x0B } })
            }
        }
    }
}
ASL
echo "ASL created"

echo ""
echo "=== 2. Compile to AML ==="
iasl /tmp/wltios_dsdt.asl 2>&1
ls -la /tmp/wltios_dsdt.aml
echo "AML size: $(stat -c%s /tmp/wltios_dsdt.aml) bytes"

echo ""
echo "=== 3. Generate C header from AML ==="
# Convert AML binary to C byte array
python3 << 'PYEOF'
with open("/tmp/wltios_dsdt.aml", "rb") as f:
    aml = f.read()

print(f"AML size: {len(aml)} bytes")
# Generate C code
c_code = "/* WLTIOS PCI host bridge DSDT AML bytecode (compiled from wltios_dsdt.asl).\n"
c_code += " * Defines _SB.PCI0 with _HID=PNP0A03 so the Linux kernel's ACPI PCI\n"
c_code += " * root bridge driver triggers a PCI bus scan. Without this, the\n"
c_code += " * kernel's PCI subsystem initializes but never scans for devices. */\n"
c_code += f"static const unsigned char wltios_dsdt_aml[{len(aml)}] = {{\n"
for i in range(0, len(aml), 12):
    chunk = aml[i:i+12]
    hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
    c_code += f"    {hex_vals},\n"
c_code += "};\n"

with open("/tmp/wltios_dsdt_aml.h", "w") as f:
    f.write(c_code)
print(f"C header written: {len(c_code)} bytes")
PYEOF

echo ""
echo "=== 4. Read current DSDT code in acpi.c ==="
grep -n "DSDT\|dsdt" "$ACPIC" | head -10

echo ""
echo "=== 5. Patch acpi.c: replace empty DSDT with AML-embedded DSDT ==="
python3 << 'PYEOF'
import os

acpi_path = "/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c"
with open(acpi_path, "r") as f:
    content = f.read()

# Read the AML header
with open("/tmp/wltios_dsdt_aml.h", "r") as f:
    aml_header = f.read()

# The old DSDT code:
old_dsdt = """    // DSDT
    int dsdt_size = sizeof(acpi_dsdt_t);
    acpi_dsdt_t *dsdt = calloc(1, dsdt_size);
    assert(NULL != dsdt);

    acpi_fill_table_head(&dsdt->header, "DSDT", 1);

    dsdt->header.length = dsdt_size;
    dsdt->header.checksum = acpi_calc_checksum((char *)dsdt, dsdt_size);

    tables[num_tables] = dsdt;
    table_sizes[num_tables] = dsdt_size;

    int dsdt_index = num_tables;

    num_tables++;"""

# The new DSDT code: includes the AML blob after the header
new_dsdt = """    // DSDT — WLTIOS: includes PCI host bridge AML so kernel scans PCI bus
    extern const unsigned char wltios_dsdt_aml[];
    extern const int wltios_dsdt_aml_size;
    int dsdt_size = sizeof(acpi_dsdt_t) + sizeof(wltios_dsdt_aml);
    acpi_dsdt_t *dsdt = calloc(1, dsdt_size);
    assert(NULL != dsdt);

    acpi_fill_table_head(&dsdt->header, "DSDT", 2);
    /* Copy AML bytecode after the DSDT header */
    memcpy((char *)dsdt + sizeof(acpi_dsdt_t), wltios_dsdt_aml, sizeof(wltios_dsdt_aml));

    dsdt->header.length = dsdt_size;
    dsdt->header.checksum = acpi_calc_checksum((char *)dsdt, dsdt_size);

    tables[num_tables] = dsdt;
    table_sizes[num_tables] = dsdt_size;

    int dsdt_index = num_tables;

    num_tables++;"""

if old_dsdt in content:
    content = content.replace(old_dsdt, new_dsdt)
    # Also add the AML array at the top of the file (after includes)
    # Use the inline array definition
    aml_inline = aml_header.replace("static const unsigned char wltios_dsdt_aml", 
                                     "const unsigned char wltios_dsdt_aml")
    # Add after the last #include
    last_include = content.rfind("#include <")
    if last_include != -1:
        end_of_include = content.find("\n", last_include) + 1
        content = content[:end_of_include] + "\n" + aml_inline + "\n" + content[end_of_include:]
    
    with open(acpi_path, "w") as f:
        f.write(content)
    print("Patched acpi.c with PCI host bridge DSDT")
else:
    print("ERROR: Could not find old DSDT code to replace")
    # Try to find it with different whitespace
    if "DSDT" in content:
        print("DSDT found in content but exact match failed")
    else:
        print("DSDT not found at all")
PYEOF

echo ""
echo "=== 6. Verify patch ==="
grep -n "wltios_dsdt_aml\|PCI host bridge" "$ACPIC" | head -5

echo ""
echo "=== 7. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5

echo ""
echo "=== 8. Boot test ==="
LOG=/home/kushal/torpd-a2-dsdt-pci.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 9. Check PCI enumeration ==="
echo "--- PCI scan messages ---"
strings "$LOG" | grep -i "pci 0000\|pci_bus\|00:0\|pci.*device\|pci.*scan" | head -15
echo "--- /sys/bus/pci ---"
strings "$LOG" | grep "\[pci\]" | head -15
echo "--- IPC ---"
strings "$LOG" | grep "\[ipc\]" | head -10
