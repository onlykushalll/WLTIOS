#!/bin/bash
# Read the VMM's CPUID handler to find where it strips AVX2/BMI2/FMA features.
echo "=== 1. Read cpuid.c (VMM CPUID handler) ==="
cat /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c 2>/dev/null

echo ""
echo "=== 2. Read cpuid.h ==="
head -50 /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.h 2>/dev/null

echo ""
echo "=== 3. Check the feature mask ==="
grep -rn "feature_mask\|cpuid_mask\|hide_feature\|strip" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/ \
    2>/dev/null | head -10

echo ""
echo "=== 4. Check vm_cpuid_handler ==="
grep -A30 "vm_cpuid_handler" \
    /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c \
    2>/dev/null | head -35
