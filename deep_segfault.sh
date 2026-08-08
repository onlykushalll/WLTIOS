#!/bin/bash
# Check if the CPUID fix actually changed the guest's view + dig deeper into segfault.
LOG=/home/kushal/torpd-cpuid-fix.log

echo "=== 1. Check CPUID-related kernel messages ==="
strings "$LOG" | grep -i "NX\|Execute Disable\|flags\|feature\|CPUID\|avx\|bmi\|fma\|xsave" | head -10

echo ""
echo "=== 2. Check the segfault details ==="
strings "$LOG" | grep "segfault" | head -3

echo ""
echo "=== 3. Check if Tor produces any output before segfault ==="
strings "$LOG" | grep -B2 -A2 "tor\[" | head -15

echo ""
echo "=== 4. Check tor-pd messages ==="
strings "$LOG" | grep "tor-pd\|tor.*pid\|TOR_EXEC\|Starting Tor" | head -10

echo ""
echo "=== 5. Check if there's a tor.log ==="
strings "$LOG" | grep -A5 "WLTIOS-TOR-LOG" | head -10

echo ""
echo "=== 6. Check if vm1 wlt-shell actually runs ==="
strings "$LOG" | grep "vm1\|wlt-shell.*vm1" | head -10

echo ""
echo "=== 7. Read the cpuid.c case 1 handler (leaf 1.ecx) ==="
sed -n '/case 1:/,/break;/p' /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c 2>/dev/null | head -20

echo ""
echo "=== 8. Check if XSAVE is needed for AVX ==="
# glibc requires XSAVE+OSXSAVE to use AVX. If we add AVX to leaf 1
# without XSAVE, glibc still won't use it. But if we DON'T advertise
# AVX at all, glibc should fall back to SSE. The segfault might be
# from a different cause entirely.
echo "The segfault at ip=(null) means a null function pointer call."
echo "This could be from glibc IFUNC dispatch OR from Tor's own CPU detection."
echo "Need to check if Tor binary has its own CPUID checks."
