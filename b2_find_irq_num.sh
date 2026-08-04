#!/bin/bash
# Find CROSS_VM_EVENT_IRQ_NUM definition.
echo "=== 1. Find CROSS_VM_EVENT_IRQ_NUM ==="
grep -rn "CROSS_VM_EVENT_IRQ_NUM" /home/kushal/wltios-build/projects/vm/ 2>/dev/null | head -10

echo ""
echo "=== 2. Check the camkes-generated config ==="
grep -rn "CROSS_VM_EVENT_IRQ\|crossvm.*irq\|CROSS_VM_IRQ" /home/kushal/wltios-build/build-tor-pd-vm1/ 2>/dev/null | grep -v ".o\|Binary" | head -15

echo ""
echo "=== 3. Check the generated vm0 camkes header ==="
grep -rn "CROSS_VM\|crossvm\|IRQ_NUM\|irq_num" /home/kushal/wltios-build/build-tor-pd-vm1/vm0/include/ 2>/dev/null | head -15

echo ""
echo "=== 4. Check the VM configuration for IRQ settings ==="
grep -rn "irq.*num\|IRQ_NUM\|irq_num\|cross_vm.*irq\|intready" /home/kushal/wltios-build/projects/vm/components/VM/configurations/ 2>/dev/null | head -15

echo ""
echo "=== 5. Check the connections.h for cross_vm IRQ ==="
cat /home/kushal/wltios-build/projects/vm/components/VM/configurations/connections.h 2>/dev/null | head -60

echo ""
echo "=== 6. Check optiplex9020 for CROSS_VM_EVENT_IRQ_NUM ==="
grep -rn "CROSS_VM_EVENT_IRQ_NUM\|crossvm_irq" /home/kushal/wltios-build/build-optiplex9020/ 2>/dev/null | head -5
# If optiplex9020 isn't built, check its generated config
find /home/kushal/wltios-build -name "*.h" -path "*optiplex*" 2>/dev/null | head -5
