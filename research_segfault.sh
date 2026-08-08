#!/bin/bash
# Deep research: Tor segfaults at ip=(null) — what does this mean?
# ip=(null) means the instruction pointer is 0 — a null function pointer call.
# This is NOT the ISA check (that would show a real address).
# The xor eax,eax;ret fix made the ISA check pass, but Tor crashes later
# calling through a null function pointer — likely a CPUID-based feature
# detection that returns wrong results because the VMM doesn't expose
# AVX2/BMI2/FMA CPUID leaves.
#
# Research: what CPUID features does Tor/glibc check at runtime?
echo "=== 1. Check if glibc has runtime CPU feature dispatch ==="
# glibc uses IFUNC resolvers that check CPUID and dispatch to optimized
# functions (AVX2, SSE4.2, etc). If the VMM doesn't expose these features
# in CPUID, glibc dispatches to a NULL pointer.
strings /home/kushal/wltios-build/wltios-rootfs64/lib/libc.so.6 | grep -i "avx\|bmi\|fma\|sse4\|ifunc\|resolve" | head -10

echo ""
echo "=== 2. Check what CPUID features the guest sees ==="
# The guest kernel logs CPU features — check if AVX2/BMI2 are present
LOG=/home/kushal/torpd-tor-first.log
strings "$LOG" | grep -i "flags\|cpu\|feature\|avx\|bmi\|fma\|sse" | head -15

echo ""
echo "=== 3. Check the guest /proc/cpuinfo ==="
# We can't read /proc/cpuinfo from the guest, but the kernel logs
# CPU model and features at boot
strings "$LOG" | grep "CPU:" | head -3

echo ""
echo "=== 4. Check if the VMM emulates CPUID ==="
# The seL4 VMM traps CPUID instructions and can modify the results
grep -rn "cpuid\|CPUID\|cpu_features\|feature_mask" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/ \
    /home/kushal/wltios-build/projects/vm/components/Init/src/ \
    2>/dev/null | head -20

echo ""
echo "=== 5. Check if there's a CPUID handler in the VMM ==="
grep -rn "cpuid\|CPUID" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/ \
    2>/dev/null | head -15

echo ""
echo "=== 6. Read the VMM's CPUID handling code ==="
find /home/kushal/wltios-build/projects -name "*.c" -path "*x86*" | xargs grep -l "cpuid\|CPUID" 2>/dev/null | head -10

echo ""
echo "=== 7. Check if -cpu host passes through CPUID ==="
# With KVM -cpu host, CPUID should pass through real CPU features
# But the VMM might intercept CPUID
strings "$LOG" | grep "CPU:" | head -3
