#!/bin/bash
# Check what config attributes the optiplex9020 uses for dataports.
echo "=== optiplex9020 dataport config ==="
grep -E "dp1_id|dp1_size|dp2_id|dp2_size|tor_src_dp|tor_dest_dp" /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null | head -10

echo ""
echo "=== tor_pd_vm1 dataport config (current) ==="
grep -E "dp.*id|dp.*size|tor_src_dp|tor_dest_dp" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>/dev/null | head -10

echo ""
echo "=== Check VM_PER_VM_CONFIG_DEF macro ==="
grep -A10 "VM_PER_VM_CONFIG_DEF" /home/kushal/wltios-build/projects/vm/configurations/vm.h 2>/dev/null | head -20

echo ""
echo "=== Check how dp_id is used in the Init component ==="
grep -rn "dp.*_id\|dataport.*id\|tor_src_dp_id" /home/kushal/wltios-build/projects/vm/components/Init/ 2>/dev/null | head -15

echo ""
echo "=== Check the generated camkes config for tor_src_dp ==="
grep -rn "tor_src_dp\|tor_dest_dp" /home/kushal/wltios-build/build-tor-pd-vm1/camkes-gen.cmake 2>/dev/null | head -10
grep -rn "tor_src_dp\|tor_dest_dp" /home/kushal/wltios-build/build-tor-pd-vm1/minimal_tor_pd.cdl 2>/dev/null | head -10
