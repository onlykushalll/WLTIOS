#!/bin/bash
# Fix: Pass through AVX2/BMI2/FMA/XSAVE/NX from host CPU in VMM CPUID handler.
# The VMM does native_cpuid() first (reads real host values), then &= with mask.
# Adding these to the mask lets them pass through since i5-1135G7 supports them.
set +e
CPUID_C=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/processor/cpuid.c

echo "=== 1. Read current masks ==="
echo "--- word4 (CPUID 1.ecx) ---"
grep -A5 "kvm_supported_word4_x86_features" "$CPUID_C" | head -8
echo "--- word9 (CPUID 7.0.ebx) ---"
grep -A3 "kvm_supported_word9_x86_features" "$CPUID_C" | head -5
echo "--- word1 (CPUID 0x80000001.edx) ---"
grep -A5 "kvm_supported_word1_x86_features" "$CPUID_C" | head -8

echo ""
echo "=== 2. Fix: add XSAVE, OSXSAVE, AVX, FMA to word4 ==="
# Current: 0 /*F(AES)*/ | 0/*F(XSAVE)*/ | 0/*F(OSXSAVE)*/ | 0 /*F(AVX)*/
# New:     0 /*F(AES)*/ | F(XSAVE) | F(OSXSAVE) | F(AVX)
# Also add F(FMA) which was 0
sed -i 's|0 \/\*F(AES)\*\/ | 0\/*F(XSAVE)\*\/ | 0\/*F(OSXSAVE)\*\/ | 0 \/\*F(AVX)\*\/|0 \/\*F(AES)\*\/ | F(XSAVE) | F(OSXSAVE) | F(AVX) |' "$CPUID_C"
# Also add FMA - change 0 /*F(FMA)*/ to F(FMA)
sed -i 's|0 \/\*F(FMA)\*\/|F(FMA)|' "$CPUID_C"

echo ""
echo "=== 3. Fix: restore AVX2, BMI1, BMI2 in word9 ==="
# I stripped them last attempt — restore to pass through
sed -i 's|0 \/\*F(BMI1)\*\/|F(BMI1)|' "$CPUID_C"
sed -i 's|0 \/\*F(AVX2)\*\/|F(AVX2)|' "$CPUID_C"
sed -i 's|0 \/\*F(BMI2)\*\/|F(BMI2)|' "$CPUID_C"

echo ""
echo "=== 4. Fix: add NX to word1 ==="
# Current: 0 /*F(NX)*/ | F(LM) | F(GBPAGES) | F(SYSCALL)
# Change 0 /*F(NX)*/ to F(NX)
sed -i 's|0 \/\*F(NX)\*\/|F(NX)|' "$CPUID_C"

echo ""
echo "=== 5. Verify changes ==="
echo "--- word4 (should have F(XSAVE) F(OSXSAVE) F(AVX) F(FMA)) ---"
grep -A5 "kvm_supported_word4_x86_features" "$CPUID_C" | head -8
echo "--- word9 (should have F(BMI1) F(AVX2) F(BMI2)) ---"
grep -A3 "kvm_supported_word9_x86_features" "$CPUID_C" | head -5
echo "--- word1 (should have F(NX)) ---"
grep -A5 "kvm_supported_word1_x86_features" "$CPUID_C" | head -8

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -3

echo ""
echo "=== 7. Boot (90s) ==="
LOG=/home/kushal/torpd-cpuid-passthrough.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 8. Results ==="
echo "--- NX/flags ---"
strings "$LOG" | grep -i "NX\|Execute Disable\|flags" | head -5
echo "--- Tor ---"
strings "$LOG" | grep "tor-pd\|^PID.*tor\|tor\[\|wlt-check\|Starting Tor" | head -10
echo "--- Tor log ---"
strings "$LOG" | grep -A15 "WLTIOS-TOR-LOG" | head -20
echo "--- segfault ---"
strings "$LOG" | grep "segfault" | head -3
echo "--- Bridge ---"
strings "$LOG" | grep "\[bridge\]" | head -10
echo "--- RESULT ---"
strings "$LOG" | grep "RESULT:" | head -3
echo "--- VMs ---"
echo "Linux: $(strings "$LOG" | grep -c 'Linux version')"
