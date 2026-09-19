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
PATCH_DIR="$(cd "$(dirname "$0")" && pwd)"  # absolute: later blocks cd around

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

# Patch 0002: PCI host-bridge DSDT with _PRT (00:01.0 INTA -> GSI 11)
# REGENERATED 2026-09-19 from the proven build tree. The previously
# committed version embedded a 36-byte DSDT table header inside the AML
# array -- acpi_fill_table_head() then wrapped a SECOND header around it,
# the guest kernel parsed the ASCII "DSDT" magic as AML opcodes, and PCI0
# never materialised (no root bridge, no bus scan, pcnet32 never bound).
# This version embeds the bare 138-byte AML body plus the _PRT routing
# table so the pcnet's INT A resolves to GSI 11.
# NOTE: this block was previously MISSING from apply-patches.sh entirely
# (the script jumped 0001 -> 0003): fresh checkouts applied no DSDT at all.
if [ -d "$SEL4_LIBS" ]; then
    if [ -f "$PATCH_DIR/0002-libsel4vmmplatsupport-add-PCI-host-bridge-DSDT.patch" ]; then
        cd "$SEL4_LIBS"
        if git apply --check "$PATCH_DIR/0002-libsel4vmmplatsupport-add-PCI-host-bridge-DSDT.patch" 2>/dev/null; then
            git apply "$PATCH_DIR/0002-libsel4vmmplatsupport-add-PCI-host-bridge-DSDT.patch"
            echo "[apply-patches] Applied: 0002-libsel4vmmplatsupport-add-PCI-host-bridge-DSDT"
        else
            echo "[apply-patches] Already applied or conflict: 0002-libsel4vmmplatsupport-add-PCI-host-bridge-DSDT"
        fi
    fi
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

# Patch 0004: libsel4vm direct external-interrupt vector injection (RC9 fix)
# Stock vm_inject_irq drives the emulated i8259 vPIC -- structurally dead
# for an ioapic-mode guest (EOIs go to the real VTX APIC, never the
# emulation, so in-service vectors wedge and priority-block same-class
# deliveries forever). Adds vm_apic_inject_external_vector(): direct
# VM-entry injection at the guest-programmed vector (0x30+irq), a
# pending-vector queue delivered at the next interrupt window, and a
# post-delivery re-ack hook. Proven: pcnet IRQ 11 delivered as vector
# 59, TX watchdog timeouts gone, cross-vm bridge ping-pong SUCCESS.
if [ -d "$SEL4_LIBS" ]; then
    if [ -f "$PATCH_DIR/0004-libsel4vm-rc9-direct-irq-injection.patch" ]; then
        cd "$SEL4_LIBS"
        if git apply --check "$PATCH_DIR/0004-libsel4vm-rc9-direct-irq-injection.patch" 2>/dev/null; then
            git apply "$PATCH_DIR/0004-libsel4vm-rc9-direct-irq-injection.patch"
            echo "[apply-patches] Applied: 0004-libsel4vm-rc9-direct-irq-injection"
        else
            echo "[apply-patches] Already applied or conflict: 0004-libsel4vm-rc9-direct-irq-injection"
        fi
    fi
fi

# Patch 0005: vm Init template consumes the RC9 injection API
# Inject loop calls vm_apic_inject_external_vector(0x30+irq), caches
# per-irq handler caps, defines vm_rc9_reack_vector() -- host IRQ
# re-acked ONLY after actual guest delivery (self-clocking level line;
# re-acking at inject time starves the guest with an infinite badge
# storm). Also carries the guest_fake_devices memory-reservation hunk
# from the eth0-binding stack (previously build-tree-only).
if [ -d "$VM_PROJECTS" ]; then
    if [ -f "$PATCH_DIR/0005-vm-Init-rc9-irq-injection.patch" ]; then
        cd "$VM_PROJECTS"
        if git apply --check "$PATCH_DIR/0005-vm-Init-rc9-irq-injection.patch" 2>/dev/null; then
            git apply "$PATCH_DIR/0005-vm-Init-rc9-irq-injection.patch"
            echo "[apply-patches] Applied: 0005-vm-Init-rc9-irq-injection"
        else
            echo "[apply-patches] Already applied or conflict: 0005-vm-Init-rc9-irq-injection"
        fi
    fi
fi

# ---------- Camkes app sync + drift guard (gap #26 recurrence) ----------
# The tracked camkes app is AUTHORITATIVE. Missing build-tree copies are
# bootstrapped from git; an existing copy that has drifted (hand-edited
# build tree without a matching commit) FAILS the build setup.
VM_EXAMPLES="${3:-$(dirname "$VM_PROJECTS")/vm-examples}"
APP_SRC="$PATCH_DIR/../../src/sel4-partitions/tor-pd/camkes/app"
LIVE_APP="$VM_EXAMPLES/apps/x86/tor_pd_vm1"

if [ -d "$LIVE_APP" ]; then
    for f in minimal_tor_pd.camkes src/cross_vm_connections_vm1.c src/cross_vm_connections.c app_settings.cmake CMakeLists.txt; do
        if [ -f "$APP_SRC/$f" ] && [ ! -f "$LIVE_APP/$f" ]; then
            cp "$APP_SRC/$f" "$LIVE_APP/$f"
            echo "[apply-patches] Bootstrapped from git: $f"
        fi
    done
    export WLTIOS_BUILD_TREE="$(cd "$(dirname "$VM_EXAMPLES")" && pwd)/.."
    if ! bash "$PATCH_DIR/../../scripts/check-camkes-sync.sh"; then
        echo "[apply-patches] FATAL: camkes app drift -- reconcile before building" >&2
        exit 1
    fi
else
    echo "[apply-patches] WARNING: tor_pd_vm1 app dir not found at $LIVE_APP (fresh tree?)"
fi

echo "[apply-patches] Done."
