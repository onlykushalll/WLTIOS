#!/bin/bash
cd /mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS
echo "=== .gitignore ==="
cat .gitignore 2>/dev/null | head -30
echo ""
echo "=== sel4-build ignored? ==="
git check-ignore sel4-build 2>&1
echo ""
echo "=== FirewallPD tracked? ==="
git ls-files | grep -i firewall | head -10
echo ""
echo "=== tor_pd_vm1 tracked? ==="
git ls-files | grep -i tor_pd_vm1 | head -10
echo ""
echo "=== sel4-build files tracked? ==="
git ls-files sel4-build/ | head -10
echo ""
echo "=== What dirs ARE tracked under src/? ==="
git ls-files src/ | head -10
echo ""
echo "=== Does src/sel4-partitions exist? ==="
ls src/sel4-partitions/ 2>/dev/null | head
echo ""
echo "=== Where should Firewall PD files live in tracked repo? ==="
# Check where KeyVaultPD equivalent files are tracked
git ls-files | grep -i keyvault | head -10
git ls-files | grep -i "sel4-partitions" | head -10
