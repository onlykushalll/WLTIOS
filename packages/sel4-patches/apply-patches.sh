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
