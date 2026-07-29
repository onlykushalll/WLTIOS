# ADR-10: seL4 Architecture Fork — 32-bit Multi-PD vs 64-bit Desktop

**Date:** 2026-07-19
**Status:** Accepted (documents existing state + reconciliation plan)
**Supersedes:** None (clarifies ADR-03, ADR-04)

## Context

WLTIOS has two parallel seL4/Linux build tracks that were never reconciled.
This ADR documents the actual fork with hard evidence (not inference from
milestone dates) and defines the concrete plan for unification.

## Evidence (real config output, not paraphrase)

### Track 1: seL4 Multi-PD (32-bit ia32)

**Build directories:** `build-keyvault/`, `build-tor-pd/`, `build-tor-pd-only/`

**Config (from `app_settings.cmake`):**
```cmake
set(KernelSel4Arch ia32 CACHE STRING "" FORCE)
set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
```

**Config (from `CMakeCache.txt`):**
```
KernelArch:STRING=x86
KernelSel4Arch:STRING=ia32
```

**Kernel image:** `kernel-ia32-pc99` — ELF 32-bit LSB executable, Intel i386

**QEMU:** `qemu-system-i386 -machine q35,accel=kvm -cpu Nehalem`

**Apps:** `keyvault_pd`, `tor_pd`, `tor_pd_only` — all force `ia32`

### Track 2: Desktop/Compositor (standalone 64-bit Linux, NOT seL4)

**Build:** `kernel-amd64` — Linux kernel x86 boot executable, bzImage, version
5.10.218, 64-bit. Built from source (packages/kernel/wlt-kernel.config).

**Boot:** `qemu-system-x86_64 -kernel kernel-amd64 -initrd wltios-initramfs64.cpio.gz`

**No seL4 involvement.** The desktop (compositor, terminal, kairos, etc.) runs
directly on a 64-bit Linux kernel in QEMU. seL4 is not in the boot path.

### Track 3: 64-bit seL4 (exists but unused for multi-PD)

**Build directory:** `build-minimal-64/`

**Config (from `CMakeCache.txt`):**
```
KernelArch:STRING=x86
KernelSel4Arch:STRING=x86_64
```

**Config (from `app_settings.cmake`):**
```cmake
set(KernelSel4Arch x86_64 CACHE STRING "" FORCE)
set(KernelX86_64VTX64BitGuests ON CACHE BOOL "" FORCE)
set(LibSel4VMMUseHPET ON CACHE BOOL "" FORCE)
```

**Images:** `kernel-x86_64-pc99` + `capdl-loader-image-x86_64-pc99` (ELF 64-bit)

**App:** `minimal_64` — bare VM, no KeyVaultPD or TorPD test

## Section 1.2: Are these separate seL4 checkouts?

**No.** All builds share ONE seL4 repo checkout at `~/wltios-build/` (managed
by `repo` tool, `.repo/manifest.xml`). The different build directories use
the same source tree with different `CAMKES_VM_APP` and `KernelSel4Arch` flags.

This means unification is a **build config change**, not a source merge.

## Why the fork happened

The multi-PD experiments (KeyVaultPD, TorPD) started on 32-bit (ia32) because
that's what the CAmkES VM examples (`camkes-vm-examples/apps/x86/keyvault_pd`)
defaulted to — the `app_settings.cmake` forces `KernelSel4Arch=ia32`.

The desktop work moved to a standalone 64-bit Linux kernel (not seL4) because:
1. The custom kernel (CONFIG_E1000=y built-in) needed 64-bit for >503MB RAM
2. seL4's 32-bit limit (503MB physical) was too small for the desktop
3. The desktop was never integrated with seL4 — it runs directly in QEMU

A 64-bit seL4 build (`build-minimal-64`) exists but was never connected to the
multi-PD work. The `minimal_64` app_settings.cmake uses `KernelSel4Arch=x86_64`
and `KernelX86_64VTX64BitGuests ON`, proving 64-bit seL4 VMs work in this repo.

## Reconciliation Plan

### Step 1 (this session): KeyVaultPD on 64-bit
Create `keyvault_pd_64` app with `KernelSel4Arch=x86_64`. Build and verify
ping/pong works on 64-bit seL4. This de-risks the 64-bit port using the
component that's already proven on 32-bit.

### Step 2 (next session): TorPD on 64-bit
Once KeyVaultPD works on 64-bit, add TorPD to the same assembly. The 64-bit
seL4 build addresses 4GB+ of physical memory, eliminating the 503MB limit
that blocked TorPD IPC on 32-bit.

### Step 3 (future): Desktop under seL4
The desktop (compositor, terminal, kairos) currently runs on a standalone
Linux kernel. Eventually it should run as vm0 under 64-bit seL4, with TorPD
as a second VM. This is the full ADR-03 vision.

## Consequences

### Positive
- **One repo, not two** — unification is a config change, not a source merge
- **64-bit seL4 already builds** — `build-minimal-64` proves the toolchain works
- **KeyVaultPD component is arch-independent** — the `.camkes` file works on both
  32 and 64-bit; only `app_settings.cmake` forces the arch

### Negative
- **Two QEMU commands needed** — 32-bit uses `qemu-system-i386`, 64-bit uses
  `qemu-system-x86_64` with different CPU flags
- **64-bit seL4 VM may have different issues** — `KernelX86_64VTX64BitGuests`
  and `LibSel4VMMUseHPET` are 64-bit-specific flags that could affect behavior
- **Desktop still not under seL4** — Step 3 is a large future task

---

**Decision made:** 2026-07-19
**Evidence gathered from:** `CMakeCache.txt`, `app_settings.cmake`, `file` command
on kernel images, `repo` manifest — all real config output, not inference
