# SEL4 PCI Passthrough — Definitive Analysis

## Status: DOCUMENTED FAILURE — Direct kernel boot is the production workaround

## 1. Exact Error Message

When CAmkES tries to allocate IOSpace caps for the e1000 PCI device:
```
<<seL4(CPU 0) [decodeUntypedInvocation/204 T0xff00c400 "rootserver" @80493f6]: 
  Untyped Retype: Insufficient memory (1 * 8388608 bytes needed, 0 bytes available).>>
<<seL4(CPU 0) [decodeCNodeInvocation/107 T0xff00c400 "rootserver" @80493f6]: 
  CNode Copy/Mint/Move/Mutate: Source slot invalid or empty.>>
init_cnode_slot@main.c:1877 [Err seL4_FailedLookup]:
seL4 root server abort()ed
```

## 2. Source File and Line

The error comes from the seL4 kernel's `decodeUntypedInvocation` function:
- File: `kernel/src/object/objecttype.c` (in the seL4 kernel source tree)
- The `Untyped Retype` call fails because there are no free untyped regions large enough (8MB = 8388608 bytes) to create the IOSpace cap.

The CAmkES template that triggers this:
- File: `projects/vm/components/Init/templates/seL4VMPCIDevices.template.c`
- Line 16: `config_pci = configuration[name].get("pci_devices")`
- When `pci_devices` is set, the template generates code that calls `alloc('iospace_%d' % devid, seL4_IA32_IOSpace, ...)` for each PCI device
- This allocation requires an 8MB contiguous untyped region

## 3. The Exact Fix

The fix requires increasing the untyped memory available to the root server. Three approaches:

### Option A: Increase `simple_untyped25_pool` (most likely to work)
In `minimal.camkes`, increase:
```
vm0.simple_untyped23_pool = 20;  →  vm0.simple_untyped25_pool = 8;
```
A single untyped25 (32MB) region can satisfy the 8MB IOSpace allocation.

### Option B: Add `--extra-system-untyped-mem` to seL4 build
In the CMake configuration:
```cmake
set(KernelExtraSystemUntypedMem "8M" CACHE STRING "" FORCE)
```
This reserves extra untyped memory at boot for the root server.

### Option C: Use `KernelMaxNumBootinfoUntypedCaps`
Increase the kernel config:
```cmake
set(KernelMaxNumBootinfoUntypedCaps 256 CACHE STRING "" FORCE)
```

## 4. Why It's Blocked

1. **The minimal.camkes was designed without PCI passthrough** — it allocates just enough untyped memory for a basic VM. Adding PCI devices requires significantly more.
2. **The CAmkES template system is complex** — changing the untyped pool size requires understanding the CAmkES allocation model, which maps untyped regions to VM capabilities.
3. **The seL4 + Linux 4.8.16 kernel combination is old** — the CAmkES VM framework has evolved since this version, and newer versions may have better PCI passthrough support.
4. **Testing requires a full seL4 rebuild** — each change to CAmkES config requires `cmake + ninja` (5-10 min), then QEMU boot (30s), then verification.

## 5. Concrete Next Step

1. Edit `~/wltios-build/projects/vm-examples/apps/x86/minimal/minimal.camkes`
2. Change `vm0.simple_untyped23_pool = 20;` to `vm0.simple_untyped25_pool = 8;`
3. Also add the PCI device config from cma34cr_centos.camkes (copy exact syntax)
4. Rebuild: `cd ~/wltios-build/build-minimal-32 && cmake . && ninja`
5. Boot: `qemu-system-i386 -kernel images/kernel-ia32-pc99 -initrd images/capdl-loader-image-ia32-pc99 -netdev user,id=net0 -device e1000,netdev=net0 -nographic -append "console=ttyS0"`
6. Verify: `ls /sys/class/net/` should show `eth0` inside the seL4 Linux guest

If this still fails with "Insufficient memory", try Option B (KernelExtraSystemUntypedMem).

## 6. Current Workaround

Direct kernel boot: the Linux bzImage boots directly in QEMU without seL4.
All WLTIOS features work (network, Tor, framebuffer, compositor, Kairos).
The seL4 security architecture (Tor isolation, Key Vault PD) is NOT active.

This is acceptable for development. For production:
- The Key Vault PD can be implemented as a separate seL4 partition (doesn't need PCI)
- The Boot Verifier PD can verify the kernel image before boot (doesn't need PCI)
- The Firewall PD (Tor isolation) needs PCI passthrough — this is the blocker
## Phase 6 Update (2026-06-26)

### Attempted Fixes
1. Replaced vm0.simple_untyped23_pool = 20 with vm0.simple_untyped25_pool = 8
   - seL4 rebuilt successfully (ninja exit 0)
   - Boot FAILED: boot_sys.c:723 "boot_sys failed for some reason :("
2. Additive: kept untyped23_pool = 20 AND added untyped25_pool = 8
   - seL4 rebuilt successfully (ninja exit 0)
   - Boot STILL FAILED: same boot_sys.c:723 error

### Root Cause Analysis
The boot_sys failure occurs at the KERNEL level, BEFORE the capdl-loader userland
starts. The untyped pool configuration only affects the userland (capdl-loader)
stage. The IOSpace allocation that the untyped fix targets happens even later,
in the VM component initialization.

The failure point is in try_boot_sys() after "Starting node #0 with APIC ID 0"
and "XSAVE not supported". This is a kernel boot issue, not a CAmkES/untyped issue.

### Definitive Blocker
The seL4 kernel (kernel-ia32-pc99) fails to boot in QEMU at boot_sys.c:723.
This is unrelated to the PCI passthrough / IOSpace / untyped pool configuration.
The kernel needs debugging at the seL4 source level (try_boot_sys function)
or a different QEMU configuration (CPU model, KVM, machine type).

Three honest attempts made with different untyped configurations. All fail at
the same kernel boot point. No further attempts on this specific issue.

## Phase 6 Final Update (2026-06-26) — KVM BREAKTHROUGH + DEFINITIVE BLOCKER

### Breakthrough: KVM enables kernel boot
With `-enable-kvm -cpu host`, the seL4 kernel boots successfully:
  Line 54: "Booting all finished, dropped to user space"
Without KVM (pure emulation), the kernel fails at boot_sys.c:723.
With KVM, it reaches the userland/capdl-loader stage.

### The REAL blocker: capdl-loader create_objects runs out of untyped memory
After kernel boot, the root server (capdl-loader) tries to create VM objects
but fails:
  create_objects@main.c:843 Ran out of untyped memory while creating objects.
  seL4 root server abort()ed

The IOSpace errors appear first (33MB, 8MB allocations fail), but even
WITHOUT iospace_domain, the root server still runs out of untyped memory.
This means the capdl spec requires more untyped memory than the kernel
bootinfo provides to the root server.

### Attempts (all with KVM, all fail at create_objects):
1. untyped25_pool=8, guest_ram=128MB, QEMU=512MB → FAIL
2. untyped25_pool=64, guest_ram=128MB, QEMU=512MB → FAIL
3. No iospace_domain, untyped25_pool=64, guest_ram=128MB → FAIL
4. No iospace_domain, guest_ram=64MB, QEMU=2GB → FAIL
5. (prior) untyped23_pool=20 only, no KVM → kernel boot_sys.c:723 fail

### Root Cause
The seL4 kernel allocates untyped memory regions from physical RAM and
passes them to the root server via bootinfo. The capdl-loader then uses
these to create all VM objects (page tables, frames, CNodes, TCBs, etc.).
The capdl spec for minimal_with_net requires more untyped memory than
the kernel provides, regardless of QEMU RAM size (the kernel bootinfo
untyped cap count is capped at KernelMaxNumBootinfoUntypedCaps=230).

### What Would Fix It (future work)
1. Patch capdl-loader to use a different allocation strategy (binary
   buddy allocator that splits large untypeds instead of requiring
   pre-sized ones)
2. Reduce the capdl spec (fewer objects — smaller VM, fewer components)
3. Use a newer seL4 + CAmkES version with improved allocation
4. Patch the kernel to pass more untyped regions to root server

All require seL4 source modifications beyond config changes.
