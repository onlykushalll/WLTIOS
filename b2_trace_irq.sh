#!/bin/bash
# B.2: Trace the cross_vm IRQ number issue.
echo "=== 1. Find get_crossvm_irq_num ==="
grep -rn "get_crossvm_irq_num\|crossvm_irq" /home/kushal/wltios-build/projects/vm/components/Init/src/ 2>/dev/null | head -10

echo ""
echo "=== 2. Read the get_crossvm_irq_num function ==="
grep -A20 "get_crossvm_irq_num" /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null | head -25

echo ""
echo "=== 3. Check how IRQs are configured in the camkes assembly ==="
grep -rn "irq\|IRQ\|intready\|intready_connector\|crossvm.*irq" /home/kushal/wltios-build/projects/vm-examples/apps/x86/tor_pd_vm1/minimal_tor_pd.camkes 2>/dev/null | head -10

echo ""
echo "=== 4. Check optiplex9020 IRQ config ==="
grep -rn "irq\|IRQ\|intready" /home/kushal/wltios-build/projects/vm-examples/apps/x86/optiplex9020/optiplex9020.camkes 2>/dev/null | head -10

echo ""
echo "=== 5. Check the VM_INIT_DEF for IRQ-related attributes ==="
grep -A30 "VM_INIT_DEF" /home/kushal/wltios-build/projects/vm/components/VM/configurations/vm.h 2>/dev/null | head -35

echo ""
echo "=== 6. Check the assertion in interrupt.c ==="
sed -n '30,45p' /home/kushal/wltios-build/projects/seL4_projects_libs/libsel4vm/src/arch/x86/interrupt.c 2>/dev/null

echo ""
echo "=== 7. Check init_irqs in main.c ==="
sed -n '555,590p' /home/kushal/wltios-build/projects/vm/components/Init/src/main.c 2>/dev/null

echo ""
echo "=== 8. Check how optiplex9020 configures its IRQs ==="
grep -A20 "VM_PER_VM_CONFIG_DEF" /home/kushal/wltios-build/projects/vm/components/VM/configurations/vm.h 2>/dev/null | head -25
