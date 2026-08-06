#!/bin/bash
# Fix ASL syntax + recompile + patch VMM + rebuild + boot.
set -e
ACPIC=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c

echo "=== 1. Create fixed ASL ==="
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
            Name (RES0, ResourceTemplate ()
            {
                WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x0000, 0x0CF8, 0x0CFF, 0x0000, 0x0008, ,, )
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000, 0xC0000000, 0xDFFFFFFF, 0x00000000, 0x20000000, ,, )
            })
            Method (_CRS, 0, NotSerialized)
            {
                Return (RES0)
            }
        }
    }
}
ASL
echo "ASL created"

echo ""
echo "=== 2. Compile to AML ==="
iasl /tmp/wltios_dsdt.asl 2>&1
ls -la /tmp/wltios_dsdt.aml 2>/dev/null
echo "AML size: $(stat -c%s /tmp/wltios_dsdt.aml 2>/dev/null) bytes"

echo ""
echo "=== 3. Generate C array from AML ==="
python3 << 'PYEOF'
with open("/tmp/wltios_dsdt.aml", "rb") as f:
    aml = f.read()
print(f"AML size: {len(aml)} bytes")
c_code = f"const unsigned char wltios_dsdt_aml[{len(aml)}] = {{\n"
for i in range(0, len(aml), 12):
    chunk = aml[i:i+12]
    hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
    c_code += f"    {hex_vals},\n"
c_code += "};\n"
with open("/tmp/wltios_dsdt_aml.h", "w") as f:
    f.write(c_code)
print(f"C header: {len(c_code)} bytes")
PYEOF

echo ""
echo "=== 4. Patch acpi.c ==="
python3 << 'PYEOF'
acpi_path = "/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c"
with open(acpi_path, "r") as f:
    content = f.read()

# Read AML
with open("/tmp/wltios_dsdt_aml.h", "r") as f:
    aml_code = f.read()

# Add AML array after the last #include
if "wltios_dsdt_aml" not in content:
    last_include = content.rfind("#include <")
    if last_include != -1:
        end_of_include = content.find("\n", last_include) + 1
        content = content[:end_of_include] + "\n" + aml_code + "\n" + content[end_of_include:]
    print("Added AML array")

# Replace the empty DSDT with AML-embedded DSDT
old = """    // DSDT
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

new = """    // DSDT — WLTIOS: includes PCI host bridge AML so kernel scans PCI bus
    int dsdt_size = sizeof(acpi_dsdt_t) + sizeof(wltios_dsdt_aml);
    acpi_dsdt_t *dsdt = calloc(1, dsdt_size);
    assert(NULL != dsdt);

    acpi_fill_table_head(&dsdt->header, "DSDT", 2);
    memcpy((char *)dsdt + sizeof(acpi_dsdt_t), wltios_dsdt_aml, sizeof(wltios_dsdt_aml));

    dsdt->header.length = dsdt_size;
    dsdt->header.checksum = acpi_calc_checksum((char *)dsdt, dsdt_size);

    tables[num_tables] = dsdt;
    table_sizes[num_tables] = dsdt_size;

    int dsdt_index = num_tables;

    num_tables++;"""

if old in content:
    content = content.replace(old, new)
    print("Replaced empty DSDT with AML-embedded DSDT")
elif "wltios_dsdt_aml" in content and "sizeof(wltios_dsdt_aml)" in content:
    print("Already patched")
else:
    print("ERROR: Could not find old DSDT code")
    # Show what we have
    import re
    m = re.search(r'// DSDT.*?num_tables\+\+', content, re.DOTALL)
    if m:
        print(f"Found DSDT section: {m.group()[:200]}")

with open(acpi_path, "w") as f:
    f.write(content)
print("acpi.c patched")
PYEOF

echo ""
echo "=== 5. Verify ==="
grep -n "wltios_dsdt_aml\|PCI host bridge\|DSDT.*WLTIOS" "$ACPIC" | head -5

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5

echo ""
echo "=== 7. Boot ==="
LOG=/home/kushal/torpd-a2-dsdt-fixed.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 8. Check PCI ==="
echo "--- PCI scan ---"
strings "$LOG" | grep -i "pci 0000\|00:0\|pci.*device\|pci.*scan\|PCI0\|_SB\|host bridge" | head -15
echo "--- [pci] messages ---"
strings "$LOG" | grep "\[pci\]" | head -15
echo "--- IPC ---"
strings "$LOG" | grep "\[ipc\]" | head -10
