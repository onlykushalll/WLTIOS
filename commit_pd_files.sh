#!/bin/bash
# B.1 + B.2: Copy Firewall PD + TorPD camkes files into tracked repo.
set -e
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
BUILD=$REPO/sel4-build

echo "=== B.1: Firewall PD camkes files -> tracked repo ==="
# Put camkes files in a subdirectory to avoid conflicts with existing Microkit files
mkdir -p "$REPO/src/sel4-partitions/firewall-pd/camkes"
cp "$BUILD/projects/vm/components/FirewallPD/FirewallPD.camkes" \
   "$REPO/src/sel4-partitions/firewall-pd/camkes/"
mkdir -p "$REPO/src/sel4-partitions/firewall-pd/camkes/src"
cp "$BUILD/projects/vm/components/FirewallPD/src/firewall_pd.c" \
   "$REPO/src/sel4-partitions/firewall-pd/camkes/src/"
mkdir -p "$REPO/src/sel4-partitions/firewall-pd/camkes/app"
cp "$BUILD/projects/vm-examples/apps/x86/firewall_pd_64/CMakeLists.txt" \
   "$BUILD/projects/vm-examples/apps/x86/firewall_pd_64/minimal_firewall.camkes" \
   "$REPO/src/sel4-partitions/firewall-pd/camkes/app/"
mkdir -p "$REPO/src/sel4-partitions/firewall-pd/camkes/app/src"
cp "$BUILD/projects/vm-examples/apps/x86/firewall_pd_64/src/cross_vm_connections.c" \
   "$REPO/src/sel4-partitions/firewall-pd/camkes/app/src/"
echo "Firewall PD camkes files copied to tracked repo"
find "$REPO/src/sel4-partitions/firewall-pd/camkes" -type f

echo ""
echo "=== B.2: TorPD camkes files -> tracked repo ==="
mkdir -p "$REPO/src/sel4-partitions/tor-pd/camkes/app/src"
cp "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/CMakeLists.txt" \
   "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes" \
   "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/app_settings.cmake" \
   "$REPO/src/sel4-partitions/tor-pd/camkes/app/"
cp "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/src/cross_vm_connections.c" \
   "$BUILD/projects/vm-examples/apps/x86/tor_pd_vm1/src/cross_vm_connections_vm1.c" \
   "$REPO/src/sel4-partitions/tor-pd/camkes/app/src/"
echo "TorPD camkes files copied to tracked repo"
find "$REPO/src/sel4-partitions/tor-pd" -type f

echo ""
echo "=== B.2: Also commit the rootfs build scripts (for reproducibility) ==="
# These were already committed in prior sessions but let's verify
ls "$REPO/build_64bit_rootfs.sh" "$REPO/repack_rootfs.sh" "$REPO/nuclear.sh" 2>/dev/null | head

echo ""
echo "=== Verify no conflicts with existing tracked files ==="
cd "$REPO"
git status --short src/sel4-partitions/firewall-pd/camkes/ src/sel4-partitions/tor-pd/ | head -15

echo ""
echo "=== Git add + commit ==="
git add src/sel4-partitions/firewall-pd/camkes/ src/sel4-partitions/tor-pd/
git status --short | head -15
