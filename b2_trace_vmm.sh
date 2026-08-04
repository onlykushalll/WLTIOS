#!/bin/bash
# B.2: Deep trace — is camkes_cross_vm_connections_init actually in the binary?
echo "=== 1. Check if camkes_cross_vm_connections_init is in vm0.instance.bin ==="
nm /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep "cross_vm_connections_init" | head -5
echo "--- nm exit: $? ---"

echo ""
echo "=== 2. Check if the symbol is defined (T) or weak/undefined (W/U) ==="
nm /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep "camkes_cross_vm\|cross_vm_conn" | head -10

echo ""
echo "=== 3. Check vm1.instance.bin ==="
nm /home/kushal/wltios-build/build-tor-pd-vm1/vm1.instance.bin 2>/dev/null | grep "cross_vm_connections_init" | head -5

echo ""
echo "=== 4. Check the build.ninja for cross_vm_connections.c compilation ==="
grep "cross_vm_connections" /home/kushal/wltios-build/build-tor-pd-vm1/build.ninja 2>/dev/null | head -10

echo ""
echo "=== 5. Check if cross_vm_connections.o was built ==="
find /home/kushal/wltios-build/build-tor-pd-vm1 -name "cross_vm_connections*" 2>/dev/null | head -10

echo ""
echo "=== 6. Check the optiplex9020 build for comparison ==="
# Does optiplex9020 build exist?
ls /home/kushal/wltios-build/build-optiplex9020 2>/dev/null | head -5
# If not, check if we can build it
echo "--- check optiplex9020 camkes for the Init1 component ---"
grep "Init1\|component Init1\|VM_PER_VM_COMP_DEF(1)" /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null | head -5

echo ""
echo "=== 7. Check if tor_pd_vm1's Init1 has the cross_vm_connections ==="
# The CMakeLists declares: DeclareCAmkESVM(Init1 EXTRA_SOURCES .../cross_vm_connections_vm1.c)
# Let's verify this is in the build
grep "Init1\|cross_vm_connections_vm1" /home/kushal/wltios-build/build-tor-pd-vm1/build.ninja 2>/dev/null | head -10

echo ""
echo "=== 8. Check the VMM's serial output for any ZF_LOG messages ==="
# The VMM should print ZF_LOGI messages. Check if they're suppressed.
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep -i "ZF_LOG\|Finalising\|cross_vm\|connection" | head -10

echo ""
echo "=== 9. Check the log level setting ==="
strings /home/kushal/wltios-build/build-tor-pd-vm1/vm0.instance.bin 2>/dev/null | grep -i "LOG_LEVEL\|ZF_LOG_LEVEL\|DEBUG\|INFO" | head -5
