#!/bin/bash
# Fix: Strip AVX2/BMI1/BMI2 from CPUID 7.0.ebx so glibc doesn't dispatch
# to null function pointers. The VMM already strips AVX/FMA/XSAVE from
# leaf 1, so advertising AVX2 in leaf 7 creates an inconsistent state.
set +e
CPUID_C=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c

echo "=== 1. Current CPUID 7.0.ebx mask ==="
grep -A3 "kvm_supported_word9_x86_features" "$CPUID_C" | head -5

echo ""
echo "=== 2. Fix: remove AVX2, BMI1, BMI2 from leaf 7 ==="
# Change: F(FSGSBASE) | F(BMI1) | F(HLE) | F(AVX2) | F(SMEP) | F(BMI2) | F(ERMS) | F(RTM)
# To:     F(FSGSBASE) | 0 /*F(BMI1)*/ | F(HLE) | 0 /*F(AVX2)*/ | F(SMEP) | 0 /*F(BMI2)*/ | F(ERMS) | F(RTM)
sed -i 's/F(FSGSBASE) | F(BMI1) | F(HLE) | F(AVX2) | F(SMEP) |/F(FSGSBASE) | 0 \/*F(BMI1)*\/ | F(HLE) | 0 \/*F(AVX2)*\/ | F(SMEP) |/' "$CPUID_C"
sed -i 's/F(BMI2) | F(ERMS) | 0 \/\*F(INVPCID)\*\/ | F(RTM)/0 \/*F(BMI2)*\/ | F(ERMS) | 0 \/*F(INVPCID)*\/ | F(RTM)/' "$CPUID_C"

echo "=== After fix ==="
grep -A3 "kvm_supported_word9_x86_features" "$CPUID_C" | head -5

echo ""
echo "=== 3. Also check: does the handler actually apply this mask? ==="
# Look for where word9 is applied (case 7 in the switch)
grep -A20 "case 7:" "$CPUID_C" | head -25

echo ""
echo "=== 4. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo ""
echo "=== 5. Boot test (90s) ==="
LOG=/home/kushal/torpd-cpuid-fix.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 6. Results ==="
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -15
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check" | head -10
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|WLTIOS-BOOT" | head -5
