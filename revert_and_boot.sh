#!/bin/bash
# Revert DSDT to working version (0xC0000000) + boot with 120s timeout.
set +e
ACPIC=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c

echo "=== 1. Recreate original ASL (0xC0000000-0xDFFFFFFF) ==="
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
iasl /tmp/wltios_dsdt.asl 2>&1 | tail -2

echo "=== 2. Patch acpi.c ==="
python3 << 'PYEOF'
import re
acpi_path = "/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vmmplatsupport/src/arch/x86/acpi.c"
with open(acpi_path, "r") as f:
    content = f.read()
with open("/tmp/wltios_dsdt.aml", "rb") as f:
    aml = f.read()
c_code = f"const unsigned char wltios_dsdt_aml[{len(aml)}] = {{\n"
for i in range(0, len(aml), 12):
    chunk = aml[i:i+12]
    c_code += "    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n"
c_code += "};\n"
content = re.sub(r'const unsigned char wltios_dsdt_aml\[\d+\] = \{[^}]+\};\n*', '', content)
last_include = content.rfind("#include <")
if last_include != -1:
    end_of_include = content.find("\n", last_include) + 1
    content = content[:end_of_include] + "\n" + c_code + "\n" + content[end_of_include:]
with open(acpi_path, "w") as f:
    f.write(content)
print("acpi.c reverted to original DSDT")
PYEOF

echo "=== 3. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo "=== 4. Boot (120s) ==="
LOG=/home/kushal/torpd-torfix-reverted.log
timeout 120 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo "=== 5. Results ==="
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -15
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A10 "WLTIOS-TOR-LOG" | head -15
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
