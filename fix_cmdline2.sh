#!/bin/bash
CAMKES=/home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes
sed -i 's|root=/dev/mem|root=/dev/mem init=/init|' "$CAMKES"
echo "Cmdline after fix:"
grep -A2 "VM_GUEST_CMDLINE" "$CAMKES" | head -3
echo ""
echo "=== Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5
