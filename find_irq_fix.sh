#!/bin/bash
# A.1: Find the interrupt.c file with the assert fix + determine if libsel4vm is vendored.
echo "=== 1. Find interrupt.c with the fix ==="
find /home/kushal/wltios-build/projects -name "interrupt.c" -path "*x86*" 2>/dev/null | head -5
find /home/kushal/wltios-build -name "interrupt.c" -path "*arch/x86*" 2>/dev/null | head -5

echo ""
echo "=== 2. Verify the fix is applied ==="
INTR=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/interrupt.c
grep -n "assert.*irq" "$INTR" 2>/dev/null | head -5

echo ""
echo "=== 3. Is libsel4vm a git submodule or upstream checkout? ==="
cd /home/kushal/wltios-build/projects/seL4_projects_libs 2>/dev/null
git remote -v 2>/dev/null | head -3
git log --oneline -3 2>/dev/null | head -3
echo "--- is it a git repo? ---"
git rev-parse --is-inside-work-tree 2>/dev/null || echo "NOT a git repo"

echo ""
echo "=== 4. Check the .gitmodules in the main repo ==="
cat /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/.gitmodules 2>/dev/null | head -20
# Check if seL4_projects_libs is referenced
grep -rn "seL4_projects_libs\|libsel4vm" /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/.gitmodules 2>/dev/null | head -5

echo ""
echo "=== 5. Check how the build tree gets seL4_projects_libs ==="
grep -rn "seL4_projects_libs\|libsel4vm" /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/sel4-build/CMakeLists.txt 2>/dev/null | head -5
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS -name "*.cmake" -path "*sel4*" 2>/dev/null | head -5

echo ""
echo "=== 6. Check existing vendored patches pattern ==="
ls /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/packages/ 2>/dev/null | head -10
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/packages -name "*.patch" 2>/dev/null | head -5

echo ""
echo "=== 7. Check the repo for any existing patch mechanism ==="
find /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS -name "apply-patches*" -o -name "*.patch" -o -name "patches" -type d 2>/dev/null | head -10
