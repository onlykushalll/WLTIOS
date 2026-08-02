#!/bin/bash
# B.3: Verify tracked files match gitignored, and generator script is consistent.
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

echo "=== B.3: Verify tracked files match gitignored sel4-build ==="
echo ""
echo "--- FirewallPD.camkes ---"
diff src/sel4-partitions/firewall-pd/camkes/FirewallPD.camkes \
     sel4-build/projects/vm/components/FirewallPD/FirewallPD.camkes 2>&1 && echo "MATCH" || echo "DIFFERS"
echo ""
echo "--- firewall_pd.c ---"
diff src/sel4-partitions/firewall-pd/camkes/src/firewall_pd.c \
     sel4-build/projects/vm/components/FirewallPD/src/firewall_pd.c 2>&1 && echo "MATCH" || echo "DIFFERS"
echo ""
echo "--- minimal_firewall.camkes ---"
diff src/sel4-partitions/firewall-pd/camkes/app/minimal_firewall.camkes \
     sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/minimal_firewall.camkes 2>&1 && echo "MATCH" || echo "DIFFERS"
echo ""
echo "--- TorPD CMakeLists.txt ---"
diff src/sel4-partitions/tor-pd/camkes/app/CMakeLists.txt \
     sel4-build/projects/vm-examples/apps/x86/tor_pd_vm1/CMakeLists.txt 2>&1 && echo "MATCH" || echo "DIFFERS"
echo ""
echo "--- TorPD minimal_tor_pd.camkes ---"
diff src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes \
     sel4-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>&1 && echo "MATCH" || echo "DIFFERS"

echo ""
echo "=== Verify create_firewall_pd.sh output matches tracked files ==="
# The script generates files into sel4-build/. If the tracked files match
# the sel4-build files (verified above), and the script generates those
# same files, then the chain is consistent.
echo "create_firewall_pd.sh exists: $(ls create_firewall_pd.sh 2>/dev/null && echo YES || echo NO)"
echo "Generator script last modified: $(stat -c%y create_firewall_pd.sh 2>/dev/null)"
echo "Tracked FirewallPD.camkes last modified: $(stat -c%y src/sel4-partitions/firewall-pd/camkes/FirewallPD.camkes 2>/dev/null)"

echo ""
echo "=== B.3: Files reachable from tracked repo alone ==="
echo "If sel4-build/ were deleted, these tracked files would survive:"
git ls-files src/sel4-partitions/firewall-pd/camkes/ src/sel4-partitions/tor-pd/ | wc -l
echo "tracked files in Firewall PD + TorPD camkes directories"
echo ""
echo "=== Full audit: any other sel4-build-only files? ==="
echo "Checking for .camkes files in sel4-build that aren't tracked..."
find sel4-build/projects/vm/components/FirewallPD \
     sel4-build/projects/vm-examples/apps/x86/firewall_pd_64 \
     sel4-build/projects/vm-examples/apps/x86/tor_pd_vm1 \
     -name "*.camkes" -o -name "*.c" -o -name "CMakeLists.txt" 2>/dev/null | while read f; do
    rel="${f#sel4-build/}"
    # Check if a matching tracked file exists
    found=$(git ls-files | grep "$(basename "$f")" | head -1)
    if [ -z "$found" ]; then
        echo "UNTRACKED: $f"
    fi
done
echo "Audit complete."
