#!/bin/bash
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

echo "=== TRACKED src/sel4-partitions/ ==="
git ls-files src/sel4-partitions/ | head -30
echo ""
echo "=== NEW Firewall PD files in gitignored sel4-build ==="
ls -la sel4-build/projects/vm/components/FirewallPD/ 2>/dev/null
ls -la sel4-build/projects/vm/components/FirewallPD/src/ 2>/dev/null
ls -la sel4-build/projects/vm-examples/apps/x86/firewall_pd_64/ 2>/dev/null
echo ""
echo "=== TorPD files in gitignored sel4-build ==="
ls -la sel4-build/projects/vm-examples/apps/x86/tor_pd_vm1/ 2>/dev/null
ls -la sel4-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/ 2>/dev/null
echo ""
echo "=== KeyVaultPD tracked? ==="
git ls-files | grep -i keyvault | head -10
echo ""
echo "=== Any seL4 partition files tracked? ==="
git ls-files src/sel4-partitions/ src/sel4/ | head -20
echo ""
echo "=== ALL recently-touched sel4 paths check-ignore ==="
for p in sel4-build sel4-build/projects src/sel4-partitions src/sel4; do
    echo -n "$p: "
    git check-ignore "$p" 2>/dev/null || echo "(tracked)"
done
