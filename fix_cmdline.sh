#!/bin/bash
# Add rdinit=/bin/sh to kernel cmdline to bypass /init issues.
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
echo "=== BEFORE ==="
grep "VM_GUEST_CMDLINE\|kernel_cmdline" "$CAMKES" | head -5

# Add rdinit=/bin/sh after root=/dev/mem
sed -i 's|root=/dev/mem|root=/dev/mem rdinit=/bin/sh|' "$CAMKES"

echo "=== AFTER ==="
grep "VM_GUEST_CMDLINE\|kernel_cmdline" "$CAMKES" | head -5
