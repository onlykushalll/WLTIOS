# Research: seL4 + Linux VM Integration

> **Purpose:** Foundational research for WLTIOS architecture decision.
> Determines whether seL4 can host Linux as a guest VM with direct hardware access,
> which is the load-bearing assumption of the entire WLTIOS architecture.
>
> **Author:** Researcher agent (WLTIOS)
> **Date:** 2026-06-21
> **Status:** COMPLETE — ready for ADR-02 (seL4 as base)
> **Sources cited inline; full URL list at bottom.**

---

## TL;DR — The Verdict (Read This First)

**WLTIOS's core architecture is technically viable, but with significant caveats.**

✅ **seL4 can run Linux as a guest VM on x86-64.** This is real, working, and supported by the seL4 Foundation and partners (DornerWorks, UNSW). Multiple reference projects exist.

✅ **Linux guest can access real hardware via PCI passthrough.** seL4 supports Intel VT-d IOMMU on x86-64, which is exactly the mechanism that allows a Linux guest direct (zero-copy, zero-context-switch) access to GPU, network, USB, and storage devices — the same mechanism KVM/Xen use for "VFIO passthrough".

⚠️ **However, GPU passthrough on seL4 has never been publicly demonstrated.** It works in theory (it's the same VT-d mechanism as KVM), but no one in the seL4 community has published a working NVIDIA/AMD passthrough demo on seL4. This is the #1 technical risk for WLTIOS.

⚠️ **seL4's x86-64 VMM is NOT verified.** The seL4 *kernel* has formal correctness proofs; the hypervisor extensions and the VMM userspace code do not. The 64-bit x86 VM support was added by DornerWorks in 2020 and is not covered by seL4's security proofs.

⚠️ **seL4 is not designed for desktop / general-purpose OS use today.** Both Microkit and CAmkES assume a *static* system known at build time. UNSW's "Djawula" project (formerly SMOS) is the seL4 team's own attempt at a general-purpose OS on seL4 — and it is explicitly "at an early state" and unfunded. WLTIOS would be treading similar ground.

🎯 **Recommended path:** Proceed with seL4 + Linux VM, but **de-risk the GPU passthrough question first** by attempting a minimal repro: boot seL4 + Linux on QEMU/KVM, then attempt VT-d passthrough of a trivial PCI device (a network card, not GPU). If that works, GPU is mostly a configuration problem. If it doesn't, the architecture is in serious trouble.

---

## 1. seL4's VM Support

### 1.1 What is the seL4 VM feature?

seL4 is a **microkernel**, not a hypervisor — but it has **type-1 hypervisor capabilities** built in. On x86-64, seL4 runs in Ring-0 root mode (VMX root) and uses Intel VT-x hardware virtualization. It forwards virtualization events (VM exits) to a **Virtual Machine Monitor (VMM)** that runs as a *regular userspace process* in Ring-3 root mode.

> "Yes, seL4 can run Linux in a virtual machine. At present seL4 supports this on Arm v7 and v8 platforms, as well as x86 (requiring Intel VT-x, ETP and a HPET that supports MSI delivery). To support virtual machines, seL4 itself runs as a hypervisor (x86 Ring-0 root mode or ARM hyp mode) and forwards virtualisation events to a virtual machine monitor (VMM) which performs the necessary emulations. The VMM runs de-privileged (x86 Ring-3 root mode or ARM supv mode)."
> — seL4 Foundation FAQ [[1]](https://sel4.systems/About/FAQ.html)

**Key architectural point:** The VMM is a *userspace program*. This is fundamentally different from KVM (where VMM code runs in kernel) or Xen (where the hypervisor itself is monolithic). In seL4, if the VMM is compromised, it cannot break isolation between VMs — because each VM has its own VMM with only the capabilities that VM needs.

> "seL4's virtualisation design does not share the virtualisation layer, the virtual-machine monitor (VMM), between VMs, meaning the VMM cannot break isolation even if it is compromised."
> — Makatea project description [[2]](https://trustworthy.systems/projects/OLD/makatea)

### 1.2 Is it production-ready for x86-64?

**Short answer: working but not formally verified; used in production for safety-critical embedded, NOT for desktop.**

- **64-bit x86 support was added by DornerWorks in October 2020.** Before that, only 32-bit x86 guests worked.
  - "DornerWorks added support in the seL4 kernel and VMM libraries to boot 64-bit VMs, which has been tested with a basic buildroot and centOS." [[3]](https://www.dornerworks.com/blog/64-bit-x86-architecture-on-sel4)
- **The x86-64 hypervisor extensions are NOT covered by seL4's formal proofs.**
  - "Running 64-bit VMs requires kernel changes and hypervisor extensions that are not yet backed by the proof." [[3]](https://www.dornerworks.com/blog/64-bit-x86-architecture-on-sel4)
- The verified x86-64 kernel is **16,000 SLOC**; the verified config does not include the hypervisor extensions. [[1]](https://sel4.systems/About/FAQ.html)
- **NLnet funded a 2020–2022 project ("Makatea")** to build a hardened 64-bit x86 VMM capable of hosting Qubes OS VMs, led by UNSW/Trustworthy Systems (the seL4 team itself). [[4]](https://nlnet.nl/project/seL4-64bitVMM/)
- **The seL4 Foundation's own position** (March 2025, Gernot Heiser on the seL4 devel mailing list) is that work on a general-purpose OS on seL4 exists but is "presently unfunded and as such on the back burner" — referring to the **Djawula** project (formerly SMOS). [[5]](https://trustworthy.systems/projects/smos/) [[6]](https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/thread/3XYV4RIYS2B5JYGTCVFN3VGEZOW3JJOC)

**Bottom line:** seL4 + Linux on x86-64 works for static embedded/safety systems. Nobody has shipped a desktop OS on it. WLTIOS would be among the first serious attempts.

### 1.3 What performance overhead does it add?

seL4's *kernel* is the world's fastest microkernel by IPC:

> "To the best of our knowledge, seL4 is the world's fastest microkernel on the supported processors, in terms of the usual ping-pong metric: the cost of a cross-address-space message-passing (IPC) operation." [[1]](https://sel4.systems/About/FAQ.html)

> "seL4 IPC costs are about 10–20% above the hardware limit." — seL4 whitepaper [[7]](https://sel4.systems/About/seL4-whitepaper.pdf)

For **VM overhead specifically**:

- An SAE 2024 paper ("Performance Impacts from the seL4 Hypervisor") reports overheads on ARM and x86 using synthetic and real-world benchmarks. [[8]](https://saemobilus.sae.org/papers/performance-impacts-sel4-hypervisor-2024-01-3865)
- seL4 is a **type-1 hypervisor** like Xen, not a type-2 like KVM. [[9]](https://arxiv.org/pdf/2210.04328)
- The Microkit tutorial confirms: **for passthrough devices, the VM bypasses both VMM and seL4 entirely** for memory-mapped I/O — meaning device access has **zero hypervisor overhead** at runtime.
  > "This is typically called 'pass-through' because, as you can see from the diagram, the VM bypasses both the VMM and seL4 when it accesses the device. This is ideal for performance as it means that the VMM nor seL4 consume any CPU when the VM is accessing the device. However, interrupts are still managed through seL4 and the VMM." [[10]](https://docs.sel4.systems/projects/microkit/tutorial/part4.html)

**Bottom line:** For CPU-bound workloads, seL4 adds minimal overhead (comparable to other type-1 hypervisors). For I/O workloads using passthrough, there is essentially zero overhead at runtime. Interrupts always route through seL4+VMM, which adds latency in the microsecond range per IRQ.

### 1.4 How does I/O work? (Direct hardware access vs. seL4-mediated)

seL4 supports **two I/O models for guest VMs**, mirroring KVM/Xen:

#### (a) Pass-through (direct assignment)
- A physical PCI device (or its MMIO region) is mapped directly into the guest VM's address space.
- The guest's driver talks to the device as if it owned it — **no VMM involvement** for normal MMIO reads/writes.
- DMA is constrained by **Intel VT-d IOMMU** (or AMD-Vi), which seL4 programs. This protects the rest of the system from rogue DMA.
- Interrupts still go through seL4 → VMM → guest (using VT-x posted interrupts on x86).
- seL4 docs explicitly list "seL4 IOMMU (VT-d) support" for PC99/x86. [[11]](https://docs.sel4.systems/Hardware/IA32.html)

#### (b) Virtio (paravirtualized)
- The VMM emulates a virtio PCI device.
- The guest loads a virtio driver (in Linux: `virtio_pci`, `virtio_net`, `virtio_blk`, `virtio_console`).
- The VMM (or a separate backend component) handles I/O and may route it to real hardware or to a software implementation.
- The CAmkES VMM library ships with virtio PCI, Console, Net backends. [[12]](https://docs.sel4.systems/projects/camkes-vm) [[13]](https://docs.sel4.systems/projects/virtualization/docs/libsel4vmm.html)
- The newer **sDDF (seL4 Device Driver Framework)** outperforms the CAmkES-based virtio and even outperforms Linux for network throughput on some benchmarks. [[14]](https://github.com/sel4-cap/sDDF) [[15]](https://news.ycombinator.com/item?id=39978332)

#### Which does WLTIOS want?

**Pass-through** for: GPU (critical — virtio GPU is too slow for desktop), WiFi (drivers are messy and would need to live somewhere), USB controllers, NVMe.
**Virtio** for: serial console, virtual network between VMs, virtual block for OS images.

This mirrors Qubes OS: GPU is passthrough; net/storage are virtio-backed by a service VM.

---

## 2. Existing seL4 + Linux Projects

### 2.1 Reference examples from the seL4 Foundation

The official repositories:

| Repository | Purpose | URL |
|---|---|---|
| `seL4/camkes-vm-examples` | Reference CAmkES VMM apps for ARM + x86 | [[16]](https://github.com/seL4/camkes-vm-examples) |
| `seL4/camkes-vm` | Core VMM libraries (VM_Arm, VM) | [[17]](https://github.com/seL4/camkes-vm) |
| `seL4/camkes-vm-linux` | Pre-built Linux images + rootfs | [[18]](https://github.com/seL4/camkes-vm-linux) |
| `seL4/camkes-arm-vm` | ARM-only VMM apps | [[19]](https://github.com/seL4/camkes-arm-vm) |

**Supported x86 applications** (from the camkes-vm-examples README): [[16]](https://github.com/seL4/camkes-vm-examples)

- **`minimal`** — 1 guest Linux VM (32-bit x86), buildroot RAM rootfs. The simplest possible seL4+Linux demo.
- **`minimal_64`** — 1 guest Linux VM (64-bit x86). The starting point for WLTIOS.
- **`optiplex9020`** — 2 guest Linux VMs with cross-VM connectors demonstrating shared-memory communication between a guest and a native seL4 component.
- **`cma34cr_centos`** — **The most advanced x86 example.** Runs CentOS 7 as a guest, with:
  - Passthrough Ethernet (real Intel NIC mapped into the guest, fronted by a virtio configuration)
  - Passthrough SATA storage (boots CentOS from a real flash drive)
  - Passthrough USB
  - Cross-VM connectors to a native "StringReverse" component
- **`zmq_samples`** — Messaging between VMs using ZeroMQ.

**ARM examples include:** `vm_minimal`, `vm_cross_connector`, `vm_multi` (multiple VMs), `vm_serial_server` (virtio console), `vm_virtio_net` (virtual networking).

#### Feature matrix (ARM, from camkes-vm docs)

| Platform | Guest | SMP | Multi-VM | Virtio PCI | Virtio Console | Virtio Net | Cross-VM |
|---|---|---|---|---|---|---|---|
| exynos5422 | 32-bit | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TK1 | 32-bit | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| TX1 | 64-bit | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| TX2 | 64-bit | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| ZCU102 | 64-bit | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| QEMU ARM Virt | 64-bit | ✅ | ❌ | ✅ | ✅ | (implied) | — |

[[12]](https://docs.sel4.systems/projects/camkes-vm)

### 2.2 How do existing projects handle GPU / Network / Storage?

#### GPU
- **Nobody has publicly demonstrated GPU passthrough on seL4.** This was confirmed in an April 2024 seL4 devel mailing list thread where a user ("Hugo V.C.") explicitly asked for "an out-of-the-box example of a Linux Guest with gpu passthrough enabled" and got no positive answer — only pushback about whether the Linux guest is even trusted.
  > "I'm getting mad trying to run a Firefox in a Linux Guest (see attached screenshot) and was just able to start it on a headless Linux guest server with no GPU (remote X window) but performance is horrible (not usable) so I can not even do a demo. **This is a show stopper for seL4.**" — Hugo V.C., seL4 devel list, Apr 2024 [[20]](https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/message/6IPXIW5QRP4MAD6XHICPSJVQJS4YRJ6C)
- **WLTIOS implication:** This is the single biggest open technical risk. See §4 for the technical analysis of why it *should* work even though it hasn't been shown.

#### Network
- **Two approaches demonstrated:**
  1. **Passthrough Ethernet** (cma34cr_centos app): A real Intel NIC is mapped directly into the guest. seL4 + VMM program VT-d so the NIC can only DMA into guest memory. The guest loads the standard `e1000` or `igb` driver.
  2. **Virtio Net** (vm_virtio_net app): A virtual network interface is presented to the guest. Traffic is routed to a native seL4 component (which may itself be backed by a real driver via sDDF).
- The newer **sDDF network stack** has been benchmarked to outperform Linux on the same hardware. [[15]](https://news.ycombinator.com/item?id=39978332)

#### Storage
- **Three approaches demonstrated:**
  1. **RAM-based rootfs** (minimal, optiplex9020): The Linux `rootfs.cpio` is loaded into memory at boot. Fast but volatile.
  2. **Passthrough SATA** (cma34cr_centos): A real SATA device (or whole AHCI controller) is passed through. The guest boots CentOS from a real flash drive.
  3. **Partitioned SATA** (DornerWorks extension): A single physical SATA device is partitioned at the VMM level into virtual storage devices, one per VM. "This way one VM cannot overwrite or inspect the storage space for another VM." [[3]](https://www.dornerworks.com/blog/64-bit-x86-architecture-on-sel4)
  4. **Virtio Block**: Standard virtio-blk backend, served from a seL4-side block component.

### 2.3 Boot flow: UEFI → seL4 → Linux guest

seL4 on x86 produces a **multiboot-compliant image** (kernel + modules). The standard boot flow is:

```
Power on
   ↓
UEFI firmware (or legacy BIOS)
   ↓
GRUB 2  (loaded by UEFI as a bootloader)
   ↓ menuentry "seL4 VM" {
       multiboot /boot/sel4kernel
       module /boot/sel4rootserver   ← the capDL loader (VMM image goes here)
     }
   ↓
seL4 kernel boots (Ring 0, VMX root mode if VT-x enabled)
   ↓
seL4 starts the root server (capdl-loader)
   ↓
capdl-loader reads the system description and spawns components:
   • VMM process(es)
   • Native seL4 components (e.g. Tor partition, serial server, network stack)
   ↓
VMM initializes the guest: allocates guest RAM, loads bzImage + initrd
   ↓
VMM does VM entry (VMLAUNCH on Intel VT-x), guest Linux starts in Ring 0 non-root
   ↓
Linux boots, loads drivers, mounts rootfs, runs init
   ↓
WLTIOS userspace runs inside the Linux guest
```

- The seL4 docs include the GRUB2 stanza: [[11]](https://docs.sel4.systems/Hardware/IA32.html)
  ```
  menuentry "Load seL4 VM" --class os {
      load_video
      insmod gzio
      insmod part_msdos
      insmod ext2
      set root='(hd0,msdos2)'
      multiboot /boot/sel4kernel
      module /boot/sel4rootserver
  }
  ```
- For ARM, the boot is via U-Boot loading an EFI application (`bootefi`).
- For QEMU x86, the typical invocation is `qemu-system-x86_64 -kernel images/kernel-x86_64-pc99 -initrd images/capdl-loader-image-x86_64-pc99 -enable-kvm` (KVM required because seL4 itself needs VT-x).

---

## 3. Alternative: seL4 as Hypervisor with Multiple Linux VMs (Qubes-style)

### 3.1 Qubes OS background

Qubes OS is a security-focused desktop OS that runs Linux (and Windows) VMs as "qubes" on top of the **Xen hypervisor**. Each qube is a full VM with its own filesystem, network, and display. The user's workspace is split: e.g. `work` (green), `personal` (yellow), `untrusted` (red) — each isolated from the others.

> "The default cubes are 'work', 'personal' and 'untrusted', that are each run in an isolated virtual machine. If you open a phishing email in your 'untrusted' cube and malware manages to make its way into this specific environment, it cannot get to 'personal' or 'work'." [[4]](https://nlnet.nl/project/seL4-64bitVMM)

**Qubes uses Xen, not seL4.** The Qubes team explicitly evaluated Xen vs. KVM vs. seL4 and concluded:

> "Why does Qubes use Xen instead of KVM or some other hypervisor? In short: we believe the Xen architecture allows for the creation of more secure systems." [[21]](https://forum.qubes-os.org/t/porting-qubes-to-hypervisors-other-than-xen-abstracting-the-functionality-early-stage/23478)

### 3.2 The Makatea Project — Qubes-on-seL4

The seL4 team themselves have explicitly stated that seL4 is the better fit for Qubes:

> "Qubes is an open source operating system designed to provide strong security for desktop computing using virtualisation to provide isolation. Qubes is based on Xen. seL4 is a much better fit for Qubes. The project is to port Qubes to seL4 (or develop an alternative Qubes-like system for seL4)." — seL4 Foundation [[22]](https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/thread/YONWS4REOA36PZHZOEFZ2ZGF5PYTVWLO)

The **Makatea project** (UNSW/Trustworthy Systems + Neutrality, funded by NLnet 2020–2022) was created to do exactly this. Key findings from the project:

> "The security of the OS can only be as good as the isolation provided by the underlying hypervisor – if that is compromised, so is the whole OS. **Problem:** Qubes OS is built on the Xen hypervisor. While much smaller than Linux-based hypervisors such as KVM, the Xen trusted computing base (TCB) is still highly complex and consists of 100s of thousands of lines of monolithic code. Serious vulnerabilities are regularly reported for Xen." [[2]](https://trustworthy.systems/projects/OLD/makatea)

> "The Qubes' Xen Security Advisory Tracker reports that 53/283 (18%) of Xen vulnerabilities over the last eight years affected Qubes." [[4]](https://nlnet.nl/project/seL4-64bitVMM)

The Makatea requirements document explicitly discusses **passthrough devices and IOMMU (VT-d on Intel, AMD-Vi on AMD)** as the mechanism for letting VMs access real hardware safely. [[23]](https://trustworthy.systems/projects/OLD/makatea/makatea_reqts.pdf)

**Makatea's status:** The public page hasn't been updated since Feb 2022. No shipped product. It appears to have been subsumed into the broader **Djawula** general-purpose-OS effort. [[5]](https://trustworthy.systems/projects/smos/)

### 3.3 Pros / Cons: Linux-as-single-guest vs. multi-VM (Qubes-style)

| Criterion | WLTIOS current plan (single Linux guest) | Qubes-style (multiple Linux guests) |
|---|---|---|
| **Hardware access** | Easier: one VM owns all hardware | Harder: must partition devices per-VM, virtio for shared devices |
| **Isolation** | Weaker: compromise of Linux guest = compromise of all apps | Stronger: each app class isolated |
| **Memory** | Lower: one kernel image, shared page cache | Higher: N kernels + N page caches |
| **Boot time** | Faster: one guest boot | Slower: N guest boots |
| **Code reuse from Qubes** | None | Significant (GUI protocol, qube manager, etc.) |
| **Complexity** | Lower | Higher (need a GUI domain, network domain, etc.) |
| **Maturity on seL4** | Both are immature, but single-VM is much closer to existing examples | Makatea was attempting this and stalled |

### 3.4 Recommendation

For WLTIOS's 1–2 year horizon, **start with single-guest (current plan)**. Reasoning:

1. Every existing seL4+Linux x86 example uses 1–2 guests. Going to Qubes-style 5+ guests is a research project, not an engineering one.
2. The Tor-in-seL4-partition goal does not require multi-Linux-VM — it requires Linux as one guest + native seL4 components. That is exactly the camkes-vm architecture.
3. Migrating to multi-VM later is incremental: add more VMM components to the system description.

But **design the system description to allow multiple VMs from day one** — the `optiplex9020` example already shows this works with two VMs.

---

## 4. The Critical Question: Can Linux on seL4 Access Real Hardware?

This is the make-or-break question for WLTIOS. Answered below per device class.

### 4.1 GPU (NVIDIA / AMD)

**Theoretical answer: YES, via VT-d passthrough.** Same mechanism as KVM/QEMU GPU passthrough (which is well-documented and works for gaming). [[24]](https://github.com/bryansteiner/gpu-passthrough-tutorial) [[25]](https://mathiashueber.com/windows-virtual-machine-gpu-passthrough-ubuntu)

**Practical answer: NEVER publicly demonstrated on seL4.** [[20]](https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/message/6IPXIW5QRP4MAD6XHICPSJVQJS4YRJ6C)

**What it would take:**

1. seL4 boots in VMX root mode with VT-d enabled.
2. The Linux guest's address space is set up by the VMM to include the GPU's PCI config space and MMIO BARs.
3. seL4 programs the VT-d IOMMU to permit the GPU to DMA only into the guest's memory.
4. The guest loads the normal `nvidia` or `amdgpu` driver.
5. The GPU's interrupts are routed via seL4 → VMM → guest (using VT-x posted interrupts for low latency).

**Known unknowns:**

- NVIDIA's proprietary driver is famously hostile to running inside VMs (it detects virtualization and refuses, unless you pass `pci=assign-busses` etc. — there are workarounds in the KVM/VFIO community).
- GPU BAR reservation above 4GB requires the VMM to support 64-bit PCI BARs in the guest — straightforward but not done in any seL4 example I found.
- The seL4 VMM needs to handle GPU-specific PCI config space accesses; this is the same kind of code that exists in crosvm and QEMU.

**WLTIOS action:** Build a proof-of-concept with a **secondary** GPU (cheap AMD card) on a test machine before committing the architecture. If that works, NVIDIA is "just" a driver-hassle problem.

### 4.2 WiFi

**Yes, via PCI passthrough.** WiFi cards are typically PCIe devices (or USB-attached). Both paths work:

- **PCIe WiFi cards** (Intel, Atheros): pass through like any other PCI device. The guest loads `iwlwifi`, `ath9k`, etc.
- **USB-attached WiFi dongles**: pass through the USB controller (or a port on a controller) — see USB section.

**Caveat:** Most modern laptops have the WiFi card wired to specific PCIe lanes that may share IOMMU group with other devices. The standard VFIO workaround (`pcie_acs_override`) works on KVM; equivalent configuration exists in seL4 but is not in any example.

### 4.3 USB

**Yes, two ways:**

1. **Whole-controller passthrough:** Pass through the entire xHCI USB controller to the guest. The guest loads `xhci-hcd` and sees all devices on that controller. This is the standard VFIO approach.
2. **Native seL4 xHCI driver + virtio USB:** A native seL4 xHCI driver (Capgemini has built one for the Microkit [[26]](https://github.com/sel4-cap/sel4-xhci) [[27]](https://sel4.systems/Summit/2023/slides/day1-11-felmeden.pdf)) handles the real hardware, and presents individual USB devices to the guest via virtio-usb.

The cma34cr_centos example already demonstrates USB passthrough for boot media. [[16]](https://github.com/seL4/camkes-vm-examples)

### 4.4 Storage (NVMe / SATA)

**Yes, fully demonstrated.**

- **SATA passthrough** is explicitly demonstrated in cma34cr_centos. [[16]](https://github.com/seL4/camkes-vm-examples)
- **NVMe passthrough** is mechanically identical to any PCI passthrough.
- **Virtio-blk** is the recommended approach for OS images — back it with a file or partition served by a native seL4 block component.

### 4.5 Display (framebuffer)

This is distinct from GPU. seL4 itself doesn't take over the display; the Linux guest either:
- Uses the GPU passthrough (then it owns the display output entirely), OR
- Uses virtio-gpu (slow, software-rendered — acceptable for serial-console-style debugging but not a desktop).

For WLTIOS's "custom Wayland compositor" plan, we need the GPU passthrough path. The compositor would run inside the Linux guest and talk to the real GPU via the passed-through driver.

### 4.6 Summary table

| Device class | Passthrough possible? | Demonstrated on seL4? | Risk for WLTIOS |
|---|---|---|---|
| **GPU (NVIDIA/AMD)** | Yes (VT-d) | ❌ Never publicly | 🔴 HIGH — must prototype |
| **WiFi (PCIe)** | Yes | ❌ Not specifically | 🟡 MEDIUM |
| **USB (xHCI)** | Yes | ✅ In cma34cr_centos | 🟢 LOW |
| **Storage (SATA)** | Yes | ✅ In cma34cr_centos | 🟢 LOW |
| **Storage (NVMe)** | Yes | ❌ Not specifically | 🟡 MEDIUM (same mechanism) |
| **Ethernet (NIC)** | Yes | ✅ In cma34cr_centos | 🟢 LOW |
| **Audio (HDA)** | Yes | ❌ Not specifically | 🟡 MEDIUM |
| **Display** | Via GPU passthrough | ❌ Never | 🔴 HIGH (same as GPU) |

**Critical conclusion:** The architecture is viable in principle, but the GPU passthrough is the load-bearing unknown. WLTIOS must de-risk this with a minimal proof-of-concept **before** committing to seL4.

---

## 5. Build + Test Flow

### 5.1 Building seL4 + Linux guest for x86-64

Based on the camkes-vm-examples README [[16]](https://github.com/seL4/camkes-vm-examples):

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt install python3 python3-pip repo git cmake ninja-build build-essential \
                 gcc-aarch64-linux-gnu  # for cross-builds if needed

# 2. Fetch the source using Google's repo tool
mkdir camkes-vm && cd camkes-vm
repo init -u https://github.com/seL4/camkes-vm-examples-manifest.git
repo sync

# 3. Create a build directory
mkdir build && cd build

# 4. Configure for x86_64 (64-bit guest)
../init-build.sh -DCAMKES_VM_APP=minimal_64
# 4b. Alternative: optiplex9020 (2 VMs with cross-VM connector)
#     ../init-build.sh -DCAMKES_VM_APP=optiplex9020

# 5. Build
ninja
```

Output artifacts (in `images/`):

- `images/kernel-x86_64-pc99` — the seL4 kernel image
- `images/capdl-loader-image-x86_64-pc99` — the capDL loader + system description + all components (VMM, native components, Linux bzImage, rootfs.cpio)

### 5.2 Booting in QEMU

**Critical:** seL4 needs VT-x itself, so QEMU must be invoked with KVM acceleration.

```bash
qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -m 2048 \
    -kernel images/kernel-x86_64-pc99 \
    -initrd images/capdl-loader-image-x86_64-pc99 \
    -serial mon:stdio \
    -nographic
```

If you don't have KVM (e.g. running in a nested VM), you can try without `-enable-kvm -cpu host` — seL4 will fall back to TCG (slow, but may work for minimal apps). The seL4 mailing list confirms: "the VM project relying on hardware virtualization features (e.g VT-x)" [[28]](https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/message/XRN4CCYNCNXIRBIJ3NWDXGNL6EWBDTDG/attachment/2/attachment.html)

The recent camkes-vm-examples README has a specific warning for QEMU builds: "vm_minimal: document/warn how to build for QEMU" (Nov 2025 commit). [[16]](https://github.com/seL4/camkes-vm-examples)

### 5.3 Expected output (verifying Linux is running as a guest)

When `minimal_64` boots successfully, you'll see:

1. seL4 kernel boot messages (very short — ~10ms of output)
2. capdl-loader output (loading system description)
3. VMM component output ("Starting VM 0" etc.)
4. Standard Linux boot messages, starting with `Booting Linux...`
5. Buildroot login prompt: `Welcome to Buildroot / buildroot login:`

If you can log in and run `uname -a`, Linux is running as a guest. From inside the guest, `lscpu` will show a single CPU; `free` will show the RAM size you allocated in the system description.

### 5.4 Verifying seL4 partition isolation

This is the key security property WLTIOS depends on. Verification approaches:

#### (a) Cross-VM connector test (built into optiplex9020)

The optiplex9020 app has a built-in demo: from `vm0`'s shell, run `string_reverse hello`. The Linux guest writes to a shared-memory dataport; a native seL4 component (StringReverse) reads it, reverses, and writes back. The guest reads `olleh`. This proves that (i) cross-VM communication works and (ii) the native component is sandboxed — it can only see what's in the shared dataport, nothing else. [[16]](https://github.com/seL4/camkes-vm-examples)

#### (b) Fault injection test

Try to make the guest access memory it doesn't own. For example, from inside the guest:
```bash
busybox devmem 0xDEADBEEF b 0x41
```
This should trigger a VM exit, seL4 sees a fault, hands it to the VMM, and the VMM kills the guest (or logs the fault). The host (seL4 + other components) survives.

#### (c) Memory isolation test

Boot two VMs (optiplex9020). From `vm0`, try to read physical memory addresses that belong to `vm1` via `/dev/mem` or `devmem`. With VT-d and proper guest memory capping, these accesses will fault. This proves VM-to-VM isolation.

#### (d) Capability audit

The capDL system description (the `.cdl` file in the app source) explicitly lists every capability each component has. Read it. If `vm0` does not have a capability to `vm1`'s memory frames, the proof says vm0 cannot read them. This is the formal guarantee.

### 5.5 Suggested WLTIOS Phase-1 milestone

Before writing any WLTIOS-specific code, demonstrate:

1. ✅ `minimal_64` boots in QEMU on Kushal's PC (QEMU 11.0.50 already installed).
2. ✅ Log into the Buildroot shell, run `uname -a`.
3. ✅ `optiplex9020` boots two VMs, run `string_reverse` from `vm0`.
4. ✅ Boot `cma34cr_centos` on real hardware (need a cma34cr board OR adapt to a common PC) — verifies real-hardware passthrough works.
5. ⚠️ **The de-risk milestone:** Adapt cma34cr_centos's passthrough configuration to pass through a real PCIe NIC on a generic x86_64 PC. If this works, the GPU passthrough path is mechanically the same and the architecture is sound.
6. 🔴 **The big unknown:** Same as #5 but with a real GPU. Plan for this to take weeks of debugging.

---

## 6. Open Questions for the WLTIOS Team

1. **What's the fallback if GPU passthrough proves infeasible on seL4?**
   - Option A: Use a custom seL4 GPU driver that hands framebuffers to a Wayland compositor in the guest (slow but works).
   - Option B: Run Wayland + wlroots *natively* on seL4 (no Linux guest for GUI), keep Linux guest only for "driver services" (storage, network).
   - Option C: Abandon seL4, fall back to a regular Linux + containers architecture (the security story weakens).

2. **Do we use Microkit or CAmkES as the seL4 framework?**
   - Microkit is the newer, simpler, seL4-Foundation-blessed path. CAmkES is older and more battle-tested for VMs. The `Transitioning from CAmkES VMM to MicroKit VMM` talk [[29]](https://www.youtube.com/watch?v=5xE24p-RauQ) suggests the community is migrating. **Recommendation: use Microkit unless there's a specific reason not to.**

3. **Where does the Tor partition live?**
   - Native seL4 component (not in the Linux guest). The Linux guest never has the Tor private key. This is the architecture's killer feature — emphasize it in the spec.

4. **How do we handle the unverified VMM code?**
   - Accept that the VMM is not formally verified. The seL4 *kernel* is verified, which means even if the VMM is compromised, it cannot escalate beyond the capabilities it was given. That's still much stronger than Xen.

5. **What's the relationship to the Djawula project?**
   - Watch it. If UNSW publishes a verifiable general-purpose OS design, WLTIOS should consider adopting it. For now, we go it alone with Linux-as-guest.

---

## 7. Recommendations / Next Actions

### Immediate (before any WLTIOS code)

1. **Write ADR-02** recording the decision: "WLTIOS uses seL4 as base, with Linux as a guest VM via the seL4 VMM, on x86-64." Cite this document.
2. **Set up the build environment on Kushal's PC** (or in WSL2):
   - Install Linux toolchain, repo, cmake, ninja
   - Install Rust (already planned)
   - Clone camkes-vm-examples
3. **Boot `minimal_64` in QEMU** — confirm the architecture works end-to-end. This is the first tangible WLTIOS milestone and should be achievable in <1 day.

### Short-term (Week 1–4 of Phase 1)

4. **Boot `optiplex9020`** — verify multi-VM and cross-VM connectors.
5. **Audit the cma34cr_centos passthrough configuration** — read every line of its CMakeLists and CAmkES ADL file. This is the template for WLTIOS hardware access.
6. **Document WLTIOS's capDL system description skeleton** — even if it's just `vm0` + `tor_partition` + `serial_server` to start. This becomes the architectural blueprint.

### Medium-term (Month 2–6)

7. **Prototype: Linux guest + native Tor partition.** Verify cross-VM communication works for a real workload (Tor forwarding packets to a virtio-net backed by sDDF, e.g.).
8. **Prototype: PCIe NIC passthrough on a generic PC.** Adapt cma34cr_centos's NIC config to common Intel hardware.
9. **Attempt: GPU passthrough.** Use a secondary AMD GPU if possible (open-source `amdgpu` driver is more VM-friendly than NVIDIA's blob).

### Long-term (Month 6+)

10. Custom Wayland compositor inside the Linux guest.
11. Custom init system (no systemd).
12. Nix-style package manager.
13. Performance tuning: hit < 3s boot, < 256MB idle RAM.

---

## Sources

| # | Source | URL |
|---|---|---|
| 1 | seL4 Foundation FAQ | https://sel4.systems/About/FAQ.html |
| 2 | Makatea: Qubes-like OS on seL4 (UNSW TS) | https://trustworthy.systems/projects/OLD/makatea |
| 3 | DornerWorks: 64-bit x86 on seL4 (Oct 2020) | https://www.dornerworks.com/blog/64-bit-x86-architecture-on-sel4 |
| 4 | NLnet: x86-64 VM Monitor for seL4 | https://nlnet.nl/project/seL4-64bitVMM |
| 5 | Djawula: Secure General-Purpose OS (UNSW TS) | https://trustworthy.systems/projects/smos/ |
| 6 | seL4 devel: "seL4 for general purpose/desktop OS?" (Mar 2025) | https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/thread/3XYV4RIYS2B5JYGTCVFN3VGEZOW3JJOC |
| 7 | seL4 whitepaper (PDF) | https://sel4.systems/About/seL4-whitepaper.pdf |
| 8 | SAE 2024-01-3865: Performance Impacts from seL4 Hypervisor | https://saemobilus.sae.org/papers/performance-impacts-sel4-hypervisor-2024-01-3865 |
| 9 | arXiv: seL4 Microkernel for virtualization use-cases | https://arxiv.org/pdf/2210.04328 |
| 10 | Microkit Tutorial Part 4: Virtual machines | https://docs.sel4.systems/projects/microkit/tutorial/part4.html |
| 11 | seL4 docs: PC99 (x86) hardware | https://docs.sel4.systems/Hardware/IA32.html |
| 12 | seL4 docs: CAmkES VMM | https://docs.sel4.systems/projects/camkes-vm |
| 13 | seL4 docs: VMM library (libsel4vmmplatsupport) | https://docs.sel4.systems/projects/virtualization/docs/libsel4vmm.html |
| 14 | seL4 Device Driver Framework (sDDF) GitHub | https://github.com/sel4-cap/sDDF |
| 15 | HN: sDDF 0.4.0 release discussion | https://news.ycombinator.com/item?id=39978332 |
| 16 | seL4/camkes-vm-examples GitHub | https://github.com/seL4/camkes-vm-examples |
| 17 | seL4/camkes-vm GitHub | https://github.com/seL4/camkes-vm |
| 18 | seL4/camkes-vm-linux GitHub | https://github.com/seL4/camkes-vm-linux |
| 19 | seL4/camkes-arm-vm GitHub | https://github.com/seL4/camkes-arm-vm |
| 20 | seL4 devel: GPU passthrough thread (Apr 2024) | https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/message/6IPXIW5QRP4MAD6XHICPSJVQJS4YRJ6C |
| 21 | Qubes OS forum: Porting Qubes to other hypervisors | https://forum.qubes-os.org/t/porting-qubes-to-hypervisors-other-than-xen-abstracting-the-functionality-early-stage/23478 |
| 22 | seL4 devel: seL4 + Qubes OS Project (Mar 2015) | https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/thread/YONWS4REOA36PZHZOEFZ2ZGF5PYTVWLO |
| 23 | Makatea Requirements (PDF) | https://trustworthy.systems/projects/OLD/makatea/makatea_reqts.pdf |
| 24 | bryansteiner/gpu-passthrough-tutorial | https://github.com/bryansteiner/gpu-passthrough-tutorial |
| 25 | GPU passthrough guide (Ubuntu+VFIO) | https://mathiashueber.com/windows-virtual-machine-gpu-passthrough-ubuntu |
| 26 | sel4-cap/sel4-xhci: Native xHCI USB driver for seL4 | https://github.com/sel4-cap/sel4-xhci |
| 27 | seL4 Summit 2023: From Zero to xHCI Driver (PDF) | https://sel4.systems/Summit/2023/slides/day1-11-felmeden.pdf |
| 28 | seL4 devel: VM examples with QEMU (lists.sel4.systems) | https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/message/XRN4CCYNCNXIRBIJ3NWDXGNL6EWBDTDG/attachment/2/attachment.html |
| 29 | YouTube: Transitioning from CAmkES VMM to MicroKit VMM | https://www.youtube.com/watch?v=5xE24p-RauQ |
| 30 | seL4 comparison page | https://sel4.systems/About/comparison.html |
| 31 | seL4 Summit 2022: Using QEMU to extend seL4 VirtIO support (PDF) | https://sel4.systems/Foundation/Summit/2022/slides/d1_08_Using_QEMU_to_extend_seL4_VirtIO_support_Hannu_Lyytinen.pdf |
| 32 | PUCPR: Integrating VirtIO and QEMU on seL4 (PDF) | https://secplab.ppgia.pucpr.br/files/papers/2023trustcom1.pdf |
| 33 | seL4 docs: Supported platforms | https://docs.sel4.systems/Hardware |
| 34 | seL4 devel: Ethernet pass-through using CAmkES VM on x86 | https://lists.sel4.systems/hyperkitty/list/devel@sel4.systems/thread/AV4I6ZHUCS5ADM265C77UOXSVN53UCLK |
| 35 | seL4 Discourse: SIG VMM — virtualization current efforts | https://sel4.discourse.group/t/sel4-virtualization-support-current-efforts/665 |
| 36 | arXiv: LionsOS (UNSW seL4-based OS) | https://arxiv.org/html/2501.06234v1 |

---

## Appendix A: Glossary

- **seL4** — formally verified microkernel; the seL4 Foundation maintains it.
- **VMM** — Virtual Machine Monitor. Userspace process on seL4 that manages a guest VM.
- **CAmkES** — Component-based Architecture for microKernel-based Embedded Systems; the older framework for building seL4 systems.
- **Microkit** — newer, simpler seL4 framework (released 2.0 in early 2025). Successor to CAmkES.
- **capDL** — Capability Description Language. The system description file that lists all seL4 components and their capabilities.
- **VT-x** — Intel Virtualization Technology for x86 (the CPU feature that lets seL4 run as a hypervisor).
- **VT-d** — Intel Virtualization Technology for Directed I/O. The IOMMU that lets seL4 constrain DMA from PCI devices.
- **VFIO** — Virtual Function I/O. The Linux framework for assigning PCI devices to VMs (used by KVM). The conceptual model seL4 follows.
- **passthrough / direct assignment** — Giving a VM direct access to a physical device via IOMMU isolation.
- **virtio** — Paravirtualized device standard; the guest loads a virtio driver and the VMM (or another component) emulates the device.
- **sDDF** — seL4 Device Driver Framework. Newer native seL4 driver framework with better-than-Linux network performance.
- **Makatea** — UNSW/NLnet project (2020–2022) to port Qubes OS concepts to seL4.
- **Djawula** (formerly SMOS) — UNSW's current attempt at a provably secure general-purpose OS on seL4. Early stage, unfunded.
- **LionsOS** — UNSW's seL4-based embedded OS (static architecture, not general-purpose).

---

## Appendix B: One-Paragraph Executive Summary for Kushal

seL4 can absolutely run Linux as a guest VM on x86-64 — this is real, working technology backed by the seL4 Foundation, DornerWorks, and UNSW. The Linux guest can access real hardware (GPU, WiFi, USB, storage) using the same Intel VT-d passthrough mechanism that KVM uses for gaming VMs, and seL4 explicitly supports VT-d. **However:** no one in the seL4 community has publicly demonstrated GPU passthrough — it's the #1 open risk. seL4's x86 hypervisor extensions are not formally verified (the kernel is; the VMM is not). And seL4 is not designed for desktop OS use today — the seL4 team's own general-purpose-OS project (Djawula) is explicitly "early stage." **Verdict:** WLTIOS's architecture is viable; build a minimal proof-of-concept first (boot seL4+Linux in QEMU, then attempt real PCI passthrough) before committing to seL4. The Tor-in-seL4-partition design — the architecture's killer feature — is well-supported by existing examples like `optiplex9020` (which demonstrates cross-VM communication between a Linux guest and a native seL4 component).
