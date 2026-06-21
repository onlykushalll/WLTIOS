# 🎉🎉🎉 seL4 + Linux FULLY BOOTED — 2026-06-21

## THE MILESTONE

**seL4 + Linux guest VM fully booted to a login prompt in QEMU!**

```
Welcome to Buildroot
buildroot login:
```

## What Worked

### The Fix: 32-bit (i386) Build

The 64-bit (x86_64) build hit an APIC panic because WSL2's KVM doesn't support `vmx-apicv-xapic` (APIC Virtualization). This feature is needed for 64-bit LAPIC emulation.

The **32-bit (i386)** path doesn't need this feature and works perfectly.

### QEMU Command (WORKING)

```bash
qemu-system-i386 \
  -machine q35,accel=kvm,kernel-irqchip=split \
  -cpu Nehalem,+vme,-pdpe1gb,+xsave,+xsaveopt,-xsavec,-fsgsbase,-invpcid,-syscall,-lm,+vmx \
  -nographic -serial mon:stdio \
  -m size=2G \
  -kernel images/kernel-ia32-pc99 \
  -initrd images/capdl-loader-image-ia32-pc99
```

**Key: NO `enforce` flag** — WSL2 KVM doesn't support all VMX features (vmx-mtf, vmx-apicv-xapic, vmx-apicv-x2apic, vmx-wbinvd-exit). Without `enforce`, QEMU warns but continues.

## Full Boot Sequence (Confirmed)

```
1. SeaBIOS starts ✅
2. seL4 boot loader starts ✅
   → Boot config: debug_port = 0x3f8
   → Detected 1 boot module(s)
   → Parsing GRUB physical memory map
   → ACPI tables detected (RSDP, RSDT, FADT, MADT)
   → 1 CPU(s) detected
   → ELF-loading userland images
   → Starting node #0 with APIC ID 0
   → Booting all finished, dropped to user space

3. capdl-loader starts ✅
   → Guest address space allocated
   → 100000+ relocs done
   → Guest boot info populated
   → Initializing guest to start running

4. Linux 4.8.16 boots ✅
   → Linux version 4.8.16
   → x86/fpu: Legacy x87 FPU detected
   → e820: BIOS-provided physical RAM map
   → Memory: 127728K/523200K available
   → Kernel command line parsed
   → PCI: Using configuration type 1
   → Netfilter, ip_tables, IPv6 initialized
   → Console [netcon0] enabled
   → Freeing unused kernel memory
   → Starting logging: OK
   → Initializing random number generator: done
   → Starting network

5. Buildroot login prompt ✅
   → Welcome to Buildroot
   → buildroot login:
```

## What Was Tried

| Config | Result |
|--------|--------|
| x86_64 + -cpu host | ❌ APIC panic (LAPIC MMIO not emulated) |
| x86_64 + -cpu Nehalem | ❌ seL4 hangs (fsgsbase not supported) |
| x86_64 + kernel-irqchip=split/on/off | ❌ All hit APIC panic |
| x86_64 + noacpi in cmdline | ❌ Still reads ACPI, same panic |
| i386 + -cpu Nehalem + enforce | ❌ QEMU aborts (vmx-apicv-xapic not supported) |
| **i386 + -cpu Nehalem (NO enforce)** | ✅ **FULL BOOT TO LOGIN!** |

## Root Cause Analysis

The 64-bit LAPIC emulation in seL4's VMM requires hardware APIC virtualization (`vmx-apicv-xapic`). This is an Intel VT-x feature that allows the CPU to handle LAPIC accesses without VM exits.

In WSL2's nested KVM:
- Basic VT-x IS available (seL4 can run guests)
- But APIC virtualization is NOT exposed (nested limitation)
- 64-bit seL4 VMM relies on APIC virtualization → fails
- 32-bit seL4 VMM uses software LAPIC emulation → works

## Significance

This proves:
1. ✅ seL4 + Linux guest architecture works (ADR-02 validated)
2. ✅ seL4 can boot Linux as a guest VM
3. ✅ Linux fully boots inside seL4 (reaches login prompt)
4. ✅ WSL2 is a viable development environment
5. ✅ The WLTIOS architecture is technically feasible

## What's Next

1. **Log into Buildroot** — type `root` at the login prompt
2. **Verify Linux works** — run commands inside the guest
3. **Test seL4 isolation** — verify the guest can't break seL4
4. **Start building WLTIOS userspace** — replace Buildroot with WLTIOS
5. **Build WLT-Boot** — our custom bootloader (replace GRUB/multiboot)
6. **Build WLT-Init** — our custom init system (replace Buildroot init)

## Build Details

| Item | Value |
|------|-------|
| Architecture | i386 (32-bit) |
| seL4 app | minimal |
| Platform | pc99 |
| Linux version | 4.8.16 (Buildroot) |
| QEMU | 10.2.1 (WSL2) |
| KVM | enabled (nested) |
| Build time | ~5 minutes (32-bit, incremental) |
| Boot time | ~4 seconds to login prompt |

---

*Achieved: 2026-06-21 8:15 PM IST*
*From architecture document to booting seL4 + Linux in ONE DAY.*
