#!/bin/bash
# A.4: Check ALL changes in the gitignored build tree that aren't tracked.
set +e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=/home/kushal/wltios-build

echo "=== 1. Check main.c diff (Init component) ==="
cd "$BUILD/projects/vm"
git diff -- components/Init/src/main.c > /tmp/main_c.patch 2>&1
echo "main.c diff: $(wc -l < /tmp/main_c.patch) lines"
cat /tmp/main_c.patch | head -30

echo ""
echo "=== 2. Check minimal_tor_pd.camkes diff ==="
cd "$BUILD/projects/vm-examples"
git diff -- apps/x86/tor_pd_vm1/minimal_tor_pd.camkes > /tmp/camkes.patch 2>&1
echo "camkes diff: $(wc -l < /tmp/camkes.patch) lines"
cat /tmp/camkes.patch | head -40

echo ""
echo "=== 3. Check CMakeLists.txt diff ==="
git diff -- apps/x86/tor_pd_vm1/CMakeLists.txt > /tmp/cmakelists.patch 2>&1
echo "CMakeLists diff: $(wc -l < /tmp/cmakelists.patch) lines"
cat /tmp/cmakelists.patch | head -20

echo ""
echo "=== 4. Check cross_vm_connections.c diffs ==="
git diff -- apps/x86/tor_pd_vm1/src/cross_vm_connections.c apps/x86/tor_pd_vm1/src/cross_vm_connections_vm1.c > /tmp/crossvm.patch 2>&1
echo "crossvm diff: $(wc -l < /tmp/crossvm.patch) lines"
cat /tmp/crossvm.patch | head -20

echo ""
echo "=== 5. Check interrupt.c (already patched, verify) ==="
cd "$BUILD/projects/seL4_projects_libs"
git diff -- libsel4vm/src/arch/x86/interrupt.c > /tmp/interrupt.patch 2>&1
echo "interrupt.c diff: $(wc -l < /tmp/interrupt.patch) lines"

echo ""
echo "=== 6. Create combined patch for all vm-examples changes ==="
cd "$BUILD/projects/vm-examples"
git diff -- apps/x86/tor_pd_vm1/ > /tmp/tor_pd_vm1_all.patch 2>&1
echo "tor_pd_vm1 all: $(wc -l < /tmp/tor_pd_vm1_all.patch) lines"

echo ""
echo "=== 7. Create patch for vm/components/Init/src/main.c ==="
cd "$BUILD/projects/vm"
git diff -- components/Init/src/main.c > /tmp/init_main.patch 2>&1
echo "Init main.c: $(wc -l < /tmp/init_main.patch) lines"

echo ""
echo "=== 8. Copy all patches to tracked repo ==="
mkdir -p "$REPO/packages/sel4-patches"
cp /tmp/tor_pd_vm1_all.patch "$REPO/packages/sel4-patches/0002-tor_pd_vm1-camkes-cmake-crossvm-changes.patch"
cp /tmp/init_main.patch "$REPO/packages/sel4-patches/0003-Init-main-c-indentation-fix.patch"
echo "Patches copied:"
ls -la "$REPO/packages/sel4-patches/"

echo ""
echo "=== 9. Check if the tracked repo already has the tor_pd_vm1 camkes files ==="
ls "$REPO/src/sel4-partitions/tor-pd/camkes/app/" 2>/dev/null | head -5
echo "--- diff tracked vs gitignored ---"
diff "$REPO/src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes" \
     "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes" 2>&1 | head -20
