#!/bin/bash
LOG=/home/kushal/torpd-c3-tunables.log

echo "=== Guest CPU flags ==="
# Check if the guest kernel logs CPU flags
strings "$LOG" | grep -i "flags\|cpuid\|features" | head -10

echo ""
echo "=== Check for AVX/BMI in guest ==="
strings "$LOG" | grep -i "avx\|bmi\|fma\|x86-64-v" | head -10

echo ""
echo "=== Kernel CPU init messages ==="
strings "$LOG" | grep "^\[    0\." | grep -i "cpu\|x86\|fpu\|feature" | head -15

echo ""
echo "=== Try: patch libc.so.6 to remove ISA check ==="
# Find the error string in libc and patch it
LIBC=/tmp/rootfs-c3-complete/lib/libc.so.6
if [ -f "$LIBC" ]; then
    echo "Finding ISA check string offset..."
    strings -t x "$LIBC" | grep "CPU ISA level" | head -3
    echo ""
    echo "Finding the check function..."
    # The check is in _dl_check_isa_level_glibc
    nm -D "$LIBC" 2>/dev/null | grep -i "isa_level\|isa_check" | head -5
    objdump -d "$LIBC" 2>/dev/null | grep -A5 "isa_level" | head -20
fi

echo ""
echo "=== Alternative: patch the ld-linux to skip ISA check ==="
LD=/tmp/rootfs-c3-complete/lib64/ld-linux-x86-64.so.2
if [ -f "$LD" ]; then
    echo "Finding ISA check in ld-linux..."
    strings -t x "$LD" | grep "CPU ISA\|ISA level" | head -3
fi

echo ""
echo "=== Check if there's a static Tor available ==="
find /home/kushal/wltios-build -name "tor" -type f 2>/dev/null | while read f; do
    echo "$f: $(file "$f" | grep -o 'statically\|dynamically') | $(file "$f" | grep -o 'ELF [0-9]*-bit')"
done
