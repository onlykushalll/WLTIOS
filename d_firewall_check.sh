#!/bin/bash
# D.1-D.2: Check Firewall PD tracked files + test packet filtering rules.
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

echo "=== D.2: Check if Firewall PD files are tracked ==="
echo "--- tracked camkes files ---"
git -C "$REPO" ls-files | grep -i "firewall" | head -15

echo ""
echo "--- old Microkit files (tracked) ---"
git -C "$REPO" ls-files src/sel4-partitions/firewall-pd/ | head -15

echo ""
echo "--- new camkes files (tracked) ---"
git -C "$REPO" ls-files src/sel4-partitions/firewall-pd/camkes/ | head -10

echo ""
echo "=== D.1: Read firewall_pd.c (packet filtering rules) ==="
cat "$REPO/src/sel4-partitions/firewall-pd/camkes/src/firewall_pd.c" 2>/dev/null | head -80

echo ""
echo "=== D.1: Read the rules (fw_evaluate_packet) ==="
grep -A20 "fw_evaluate_packet\|fw_action_t" "$REPO/src/sel4-partitions/firewall-pd/camkes/src/firewall_pd.c" 2>/dev/null | head -30
