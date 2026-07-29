# TorPD-as-vm1 Memory Collision Fix Attempt

## What was tried
Patched projects/vm/components/Init/src/main.c to make the static memory
region arrays (guest_ram_regions, guest_fake_devices, free_anonymous_regions)
vm0-only, with vm1 getting a separate anonymous region at 0x40000000.

## Result
The VMM still fails at the FIRST memory reservation (0x80000) with
"Failed to add memory reservation: Unable to allocate new reservation node"
from guest_memory.c:174. This is a ps_calloc failure in the VMM's own
malloc ops, BEFORE any guest RAM is allocated.

## Honest assessment
The two-VM camkes assembly COMPILES + LINKS (commit 9260096), proving the
framework supports two VMs. But the VMM's memory management (vm_init +
ps_io_ops) can't handle two VMs without deeper changes to how the VMM
initializes its malloc/IO ops for a second VM instance. The per-VM memory
map patch is necessary but not sufficient — the VMM init itself needs to
be made multi-VM-aware.

This is a deeper seL4 VMM internals issue, not a camkes assembly issue.
The scaffold + IPC wiring are ready; the VMM memory init is the blocker.
