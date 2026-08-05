#!/bin/bash
# A.1-A.4: Sync tracked files + create apply-patches script + commit.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=/home/kushal/wltios-build

echo "=== 1. Sync tracked minimal_tor_pd.camkes with gitignored version ==="
cp "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes" \
   "$REPO/src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes"
echo "Synced camkes"

echo ""
echo "=== 2. Verify the sync ==="
diff "$REPO/src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes" \
     "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes" 2>&1
echo "diff exit: $? (0 = identical)"

echo ""
echo "=== 3. Create apply-patches.sh script ==="
cat > "$REPO/packages/sel4-patches/apply-patches.sh" << 'APPLYSCRIPT'
#!/bin/bash
# WLTIOS seL4 patches — applied automatically during build setup.
# These patches fix issues in upstream seL4 libraries that are not yet
# merged upstream. Each patch is numbered for application order.
#
# Usage: bash packages/sel4-patches/apply-patches.sh <seL4_projects_libs_path> <vm_projects_path>
#
# Where:
#   <seL4_projects_libs_path> = path to seL4_projects_libs checkout
#   <vm_projects_path> = path to vm projects checkout (camkes-vm-linux)

set -e
PATCH_DIR="$(dirname "$0")"

SEL4_LIBS="${1:-/home/kushal/wltios-build/projects/seL4_projects_libs}"
VM_PROJECTS="${2:-/home/kushal/wltios-build/projects/vm}"

echo "[apply-patches] Applying seL4 patches..."

# Patch 0001: Relax inject_irq assertion in libsel4vm
# The cross_vm connection uses ISA IRQ 12, but inject_irq() asserts irq >= 16.
# This relaxes the assertion to irq >= 0 so the cross_vm connection can work.
if [ -d "$SEL4_LIBS" ]; then
    if [ -f "$PATCH_DIR/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch" ]; then
        cd "$SEL4_LIBS"
        if git apply --check "$PATCH_DIR/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch" 2>/dev/null; then
            git apply "$PATCH_DIR/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch"
            echo "[apply-patches] Applied: 0001-libsel4vm-relax-inject_irq-assertion"
        else
            echo "[apply-patches] Already applied or conflict: 0001-libsel4vm-relax-inject_irq-assertion"
        fi
    fi
else
    echo "[apply-patches] WARNING: seL4_projects_libs not found at $SEL4_LIBS"
fi

# Patch 0003: Init main.c indentation fix (cosmetic, from debugging session)
# This is a whitespace-only fix and can be skipped without functional impact.
if [ -d "$VM_PROJECTS" ]; then
    if [ -f "$PATCH_DIR/0003-Init-main-c-indentation-fix.patch" ]; then
        cd "$VM_PROJECTS"
        if git apply --check "$PATCH_DIR/0003-Init-main-c-indentation-fix.patch" 2>/dev/null; then
            git apply "$PATCH_DIR/0003-Init-main-c-indentation-fix.patch"
            echo "[apply-patches] Applied: 0003-Init-main-c-indentation-fix"
        else
            echo "[apply-patches] Already applied or conflict: 0003-Init-main-c-indentation-fix"
        fi
    fi
fi

echo "[apply-patches] Done."
APPLYSCRIPT
chmod +x "$REPO/packages/sel4-patches/apply-patches.sh"
echo "apply-patches.sh created"

echo ""
echo "=== 4. Remove empty 0002 patch (no diffs) ==="
rm -f "$REPO/packages/sel4-patches/0002-tor_pd_vm1-camkes-cmake-crossvm-changes.patch"
echo "Removed empty patch"

echo ""
echo "=== 5. Verify all patches ==="
ls -la "$REPO/packages/sel4-patches/"
echo ""
echo "--- 0001 patch content ---"
cat "$REPO/packages/sel4-patches/0001-libsel4vm-relax-inject_irq-assertion-for-cross-vm-ISA-IRQ.patch"

echo ""
echo "=== 6. Test: revert interrupt.c, run apply-patches, verify ==="
cd "$BUILD/projects/seL4_projects_libs"
git checkout -- libsel4vm/src/arch/x86/interrupt.c
echo "Reverted. assert before patch:"
grep "assert.*irq" libsel4vm/src/arch/x86/interrupt.c | head -1

echo ""
echo "--- Running apply-patches.sh ---"
bash "$REPO/packages/sel4-patches/apply-patches.sh" "$BUILD/projects/seL4_projects_libs" "$BUILD/projects/vm" 2>&1

echo ""
echo "assert after patch:"
grep "assert.*irq" libsel4vm/src/arch/x86/interrupt.c | head -1

echo ""
echo "=== 7. Verify camkes file is tracked + correct ==="
grep "rdinit\|untyped23_pool\|tor_src_dp_id" "$REPO/src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes" | head -5
