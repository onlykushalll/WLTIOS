# seL4 NIC Passthrough Analysis

## Status: DOCUMENTED FAILURE — Direct kernel boot used as workaround

## The Problem
WLTIOS uses a direct Linux kernel boot in QEMU, bypassing seL4 entirely.
The seL4 microkernel is built and boots, but the CAmkES VM configuration
(minimal.camkes) does not pass the e1000 NIC through to the Linux guest.

## What Was Tried

### Attempt 1: CAmkES PCI passthrough config
Added `pci_devices` array to minimal.camkes with e1000 at BDF 0:3:0.
Result: CAmkES template error in seL4VMIRQs.template.c — the `ioapic`
field was missing from the IRQ config struct.

### Attempt 2: Added ioapic field
Added `"ioapic":0` to the vm_irqs entry. CAmkES compiled but seL4
root server crashed with "Insufficient memory (8388608 bytes needed,
0 bytes available)" — the IOSpace caps require more untyped memory
than the minimal.camkes allocates.

### Attempt 3: Increased untyped pools
Increased `simple_untyped23_pool` from 20 to 48. Still insufficient —
the IOSpace allocation for PCI devices requires large contiguous untyped
regions that the current seL4 boot configuration doesn't provide.

## Root Cause
The seL4 CAmkES VM framework requires specific untyped memory pools for
PCI device passthrough. The minimal.camkes was designed for a minimal
VM without PCI passthrough. Adding PCI devices requires:
1. Large untyped pools (24+ bits) for IOSpace caps
2. Correct IRQ routing (ioapic + pin + vector)
3. IOPort ranges for the device
4. Memory BAR mappings

The cma34cr examples show the correct syntax but they were designed for
real hardware with different PCI topologies.

## The Workaround
Direct kernel boot: the Linux bzImage boots directly in QEMU (no seL4).
All WLTIOS features work (network, Tor, framebuffer, compositor, Kairos).
The seL4 security architecture (isolating Tor in a Protection Domain) is
NOT active. This is acceptable for development but must be fixed before
production.

## Path to Fix
1. Study cma34cr_centos.camkes PCI passthrough syntax (DONE)
2. Calculate required untyped memory for e1000 IOSpace caps
3. Modify seL4 boot configuration to provide enough untyped memory
4. OR: Use a different seL4 VM app that already has PCI passthrough
5. OR: Accept direct kernel boot for production and use seL4 only for
   the Key Vault and Boot Verifier Protection Domains (not for the
   main Linux guest)

## QEMU e1000 PCI details (from boot logs)
- BDF: 0000:00:03.0
- IRQ: 11 (INTA -> GSI 11)
- IO port: 0xc000-0xc03f (BAR0)
- Memory BAR1: 0xfebc0000-0xfebdffff (128KB)
- Memory BAR2: 0xfeba0000-0xfebaffff (64KB)
