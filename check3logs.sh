#!/bin/bash
cd /tmp/ovmf-test4 2>/dev/null || exit 1
for f in ovmf_a.log ovmf_b.log ovmf_c.log; do
    echo "=== $f ==="
    wc -c "$f" 2>/dev/null
    strings "$f" 2>/dev/null | grep -v "^\[" | grep -v "^$" | head -8
    echo ""
done
echo "=== WLTIOS in any log? ==="
grep -l "WLTIOS" ovmf_*.log 2>/dev/null
echo "=== BdsDxe in any log? ==="
grep -l "BdsDxe" ovmf_*.log 2>/dev/null
