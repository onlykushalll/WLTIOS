#!/bin/bash
# Revert CPUID changes — NX causes VMM hang, XSAVE needs vCPU state management.
# Keep the working state: xor eax,eax;ret + pci=realloc=off + original CPUID masks.
set +e
CPUID_C=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c

echo "=== Revert CPUID masks to original ==="
# Revert word4: remove F(FMA), restore 0 /*F(FMA)*/
sed -i 's/^        F(FMA) | 0/        0 \/*F(FMA)*\/ | 0/' "$CPUID_C"
# Revert word9: strip BMI1, AVX2, BMI2 back
sed -i 's/F(FSGSBASE) | F(BMI1) | F(HLE) | F(AVX2)/F(FSGSBASE) | 0 \/*F(BMI1)*\/ | F(HLE) | 0 \/*F(AVX2)*\//' "$CPUID_C"
sed -i 's/F(BMI2) | F(ERMS)/0 \/*F(BMI2)*\/ | F(ERMS)/' "$CPUID_C"
# Revert word1: remove NX
sed -i 's/F(NX) | F(LM)/0 \/*F(NX)*\/ | F(LM)/' "$CPUID_C"
sed -i 's/F(NX) | 0/0 \/*F(NX)*\/ | 0/' "$CPUID_C"

echo "=== Verify reverts ==="
grep -A3 "kvm_supported_word4" "$CPUID_C" | grep -i "fma"
grep -A3 "kvm_supported_word9" "$CPUID_C" | grep -i "bmi\|avx2"
grep -A3 "kvm_supported_word1" "$CPUID_C" | grep -i "nx"

echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3
echo "REVERT_DONE"

echo ""
echo "=== Quick boot (30s) to verify VMM works again ==="
LOG=/home/kushal/torpd-cpuid-reverted.log
timeout 30 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
strings "$LOG" | grep "bridge\|RESULT\|tor" | head -5
