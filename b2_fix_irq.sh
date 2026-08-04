#!/bin/bash
# B.2 FIX: Change CROSS_VM_EVENT_IRQ_NUM from 12 to 16 (or higher).
set +e
MAIN_C=/home/kushal/wltios-build/projects/vm/components/Init/src/main.c

echo "=== 1. Current CROSS_VM_EVENT_IRQ_NUM ==="
grep -n "CROSS_VM_EVENT_IRQ_NUM" "$MAIN_C" | head -5

echo ""
echo "=== 2. Check how many IRQs are configured ==="
# Check the camkes-generated IRQ configuration
grep -rn "irq.*num\|num_irqs\|irqs_num" /home/kushal/wltios-build/build-tor-pd-vm1/vm0/include/ 2>/dev/null | head -10

echo ""
echo "=== 3. Check the irq_badges array ==="
grep -n "irq_badges" "$MAIN_C" | head -10

echo ""
echo "=== 4. Read irq_badges definition ==="
grep -B5 -A20 "irq_badges\[" "$MAIN_C" 2>/dev/null | head -30

echo ""
echo "=== 5. Fix: change CROSS_VM_EVENT_IRQ_NUM from 12 to 16 ==="
sed -i 's/#define CROSS_VM_EVENT_IRQ_NUM 12/#define CROSS_VM_EVENT_IRQ_NUM 16/' "$MAIN_C"
echo "After fix:"
grep -n "CROSS_VM_EVENT_IRQ_NUM" "$MAIN_C" | head -3

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5

echo ""
echo "=== 7. Boot ==="
LOG=/home/kushal/torpd-c2-irq-fix.log
timeout 90 qemu-system-x86_64 \
    -machine q35,accel=kvm,kernel-irqchip=split \
    -cpu host -m 4G \
    -serial mon:stdio -display none -no-reboot \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 > "$LOG" 2>&1
echo "EXIT=$? LOGSIZE=$(wc -c < "$LOG")"

echo ""
echo "=== 8. Results ==="
echo "--- Assertion errors ---"
strings "$LOG" | grep "Assertion\|assert" | head -5
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices found ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo "--- VMs booted ---"
strings "$LOG" | grep -c "Linux version"
strings "$LOG" | grep -c "RAMDISK"
echo "--- wlt-shell starting ---"
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5
