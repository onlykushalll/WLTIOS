#!/bin/bash
# Check what ISA level the glibc requires and what the guest CPU actually supports.
echo "=== WSL system glibc ISA level ==="
# Check the glibc's minimum ISA level via its symbol version
strings /lib/x86_64-linux-gnu/libc.so.6 | grep -E "GLIBC_2\.(3[0-9]|[4-9])" | sort -V | tail -5

echo ""
echo "=== Check if glibc has ISA level check ==="
strings /lib/x86_64-linux-gnu/libc.so.6 | grep "CPU ISA\|ISA level\|isa_level" | head -5

echo ""
echo "=== Check the actual CPU model + features ==="
cat /proc/cpuinfo | grep "model name" | head -1
cat /proc/cpuinfo | grep "flags" | head -1 | tr ' ' '\n' | grep -E "avx|bmi|fma|movbe|xsave" | head -10

echo ""
echo "=== Check if KVM exposes the features ==="
# The seL4 VMM may not pass through CPUID features
# Try -cpu host with explicit features
echo "Host CPU supports AVX2: $(grep -o avx2 /proc/cpuinfo | head -1)"
echo "Host CPU supports BMI2: $(grep -o bmi2 /proc/cpuinfo | head -1)"
echo "Host CPU supports FMA: $(grep -o fma /proc/cpuinfo | head -1)"
echo "Host CPU supports MOVBE: $(grep -o movbe /proc/cpuinfo | head -1)"

echo ""
echo "=== Try setting GLIBC_TUNABLES to disable ISA check ==="
# GLIBC_TUNABLES can override the ISA level check
echo "Testing: GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-BMI2,-FMA"
GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2,-BMI2,-FMA /lib64/ld-linux-x86-64.so.2 --list /home/kushal/wltios-build/wltios-rootfs64/usr/bin/tor 2>&1 | head -5

echo ""
echo "=== Check rootfs-wlt.cpio busybox (statically linked, no glibc needed) ==="
file /home/kushal/wltios-build/rootfs-wlt.cpio 2>/dev/null | head -1

echo ""
echo "=== Try: compile a STATICALLY linked Tor (no glibc dependency) ==="
# If we can find a statically linked Tor, it won't need glibc at all
echo "Checking for musl-static Tor..."
find /home/kushal/wltios-build -name "tor" -type f 2>/dev/null | while read f; do
    echo "$f: $(file "$f" | grep -o 'statically\|dynamically')"
done
