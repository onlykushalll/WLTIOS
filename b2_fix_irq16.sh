#!/bin/bash
# B.2 FIX v2: Extend irq_badges array + use IRQ 16.
set +e
MAIN_C=/home/kushal/wltios-build/projects/vm/components/Init/src/main.c
VM_H=/home/kushal/wltios-build/projects/vm/components/VM/configurations/vm.h

echo "=== 1. Revert CROSS_VM_EVENT_IRQ_NUM to 12 ==="
sed -i 's/#define CROSS_VM_EVENT_IRQ_NUM 16/#define CROSS_VM_EVENT_IRQ_NUM 12/' "$MAIN_C"
grep "CROSS_VM_EVENT_IRQ_NUM" "$MAIN_C" | head -2

echo ""
echo "=== 2. Check VM_PIC_BADGE pattern ==="
grep "VM_PIC_BADGE_IRQ" "$VM_H" 2>/dev/null | head -20

echo ""
echo "=== 3. Extend irq_badges array from [16] to [17] ==="
# Add IRQ 16 badge
sed -i 's/static seL4_Word irq_badges\[16\] = {/static seL4_Word irq_badges[17] = {/' "$MAIN_C"
# Add the 17th entry (after IRQ_15)
# VM_PIC_BADGE_IRQ_15 = 134283264 = BIT(27) | BIT(16)
# VM_PIC_BADGE_IRQ_16 = BIT(27) | BIT(17) = 134316032
sed -i 's/VM_PIC_BADGE_IRQ_15$/VM_PIC_BADGE_IRQ_15,\n    134316032  \/* BIT(27) | BIT(17) - IRQ 16 for cross_vm *\//' "$MAIN_C"

echo ""
echo "=== 4. Change CROSS_VM_EVENT_IRQ_NUM to 16 ==="
sed -i 's/#define CROSS_VM_EVENT_IRQ_NUM 12/#define CROSS_VM_EVENT_IRQ_NUM 16/' "$MAIN_C"
grep "CROSS_VM_EVENT_IRQ_NUM" "$MAIN_C" | head -2

echo ""
echo "=== 5. Verify irq_badges ==="
grep -A20 "irq_badges\[17\]" "$MAIN_C" | head -22

echo ""
echo "=== 6. Rebuild ==="
cd /home/kushal/wltios-build/build-tor-pd-vm1
ninja 2>&1 | tail -5

echo ""
echo "=== 7. Boot ==="
LOG=/home/kushal/torpd-c2-irq16.log
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
strings "$LOG" | grep "Assertion\|assert" | head -3
echo "--- IPC messages ---"
strings "$LOG" | grep "\[ipc\]" | head -20
echo "--- IPC RESULT ---"
strings "$LOG" | grep "RESULT:" | head -5
echo "--- PCI devices ---"
strings "$LOG" | grep "Found crossvm\|vendor=0x" | head -10
echo "--- VMs booted ---"
echo "Linux versions: $(strings "$LOG" | grep -c 'Linux version')"
echo "RAMDISK lines: $(strings "$LOG" | grep -c 'RAMDISK')"
echo "--- wlt-shell ---"
strings "$LOG" | grep "wlt-shell.*starting\|vm0 starting\|vm1 starting" | head -5
