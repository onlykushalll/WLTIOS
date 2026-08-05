#!/bin/bash
# A.1+A.2: Create a proper patch file for the IRQ assertion fix.
set -e
INTR=/home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/interrupt.c
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

echo "=== 1. Generate diff from the upstream version ==="
cd /home/kushal/wltios-build/projects/seL4_projects_libs
git diff -- libsel4vm/src/arch/x86/interrupt.c > /tmp/irq_assertion.patch 2>&1
echo "Patch generated: $(wc -l < /tmp/irq_assertion.patch) lines"
cat /tmp/irq_assertion.patch

echo ""
echo "=== 2. Create packages/sel4-patches/ directory ==="
mkdir -p "$REPO/packages/sel4-patches"

echo ""
echo "=== 3. Copy patch to tracked repo ==="
cp /tmp/irq_assertion.patch "$REPO/packages/sel4-patches/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch"
echo "Patch file: $(ls -la "$REPO/packages/sel4-patches/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch")"

echo ""
echo "=== 4. Verify patch applies cleanly ==="
cd /home/kushal/wltios-build/projects/seL4_projects_libs
# Revert the fix first
git checkout -- libsel4vm/src/arch/x86/interrupt.c
echo "Reverted to upstream. Checking assert:"
grep "assert.*irq" libsel4vm/src/arch/x86/interrupt.c | head -3

echo ""
echo "=== 5. Apply the patch ==="
git apply /tmp/irq_assertion.patch 2>&1
echo "Patch apply exit: $?"
echo "After patch:"
grep "assert.*irq" libsel4vm/src/arch/x86/interrupt.c | head -3

echo ""
echo "=== 6. Verify patch file is in tracked repo ==="
ls -la "$REPO/packages/sel4-patches/"

echo ""
echo "=== 7. Also check if main.c changes need to be tracked ==="
# The main.c in Init component has the CROSS_VM_EVENT_IRQ_NUM (unchanged from 12)
# but check if any other changes from the debugging session are only in gitignored tree
cd /home/kushal/wltios-build/projects/vm/components/Init/src
git diff -- main.c 2>/dev/null | head -20
echo "--- main.c diff exit: $? ---"
# Check if main.c is tracked
git status main.c 2>/dev/null | head -5
