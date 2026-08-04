#!/bin/bash
# Check how dp1_id is declared in optiplex9020's Init0 component.
echo "=== optiplex9020 Init0 component (full) ==="
sed -n '1,40p' /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null

echo ""
echo "=== optiplex9020 config section (dp1_id etc) ==="
grep -A30 "configuration" /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null | head -35

echo ""
echo "=== Check if dp1_id is a camkes attribute or a setting ==="
# In optiplex9020, Init0 has: dataport Buf(STRING_REVERSE_BUFSIZE) dp1;
# and config has: vm0.dp1_id = 1;
# The _id and _size suffixes are likely handled by the camkes dataport framework
# automatically (the attribute name is derived from the dataport name).
grep -rn "dp.*_id\|dataport.*_id\|_id.*dataport" /home/kushal/wltios-build/projects/vm/components/Init/ 2>/dev/null | head -10

echo ""
echo "=== Check how the Init component handles dataport IDs ==="
grep -rn "dp.*_id\|tor_src_dp_id\|dataport_id" /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head -15

echo ""
echo "=== Check the generated camkes header for vm0 ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "camkes-component-vm0*" -o -name "*vm0*camkes*" 2>/dev/null | head -5
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "camkes-component-*.h" 2>/dev/null | head -5

echo ""
echo "=== Check the cross_vm_connections.c for how it uses the handle ==="
cat /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/src/cross_vm_connections.c 2>/dev/null

echo ""
echo "=== Check the VMM boot log for cross_vm init ==="
# The VMM should print something when it calls camkes_cross_vm_connections_init
# Let's check if ZF_LOG messages appear
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep -i "cross_vm\|crossvm\|connection.*init\|CROSS_VM" | head -10
