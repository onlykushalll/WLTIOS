# Research: seL4 Partition Design for WLTIOS Critical Services

> **Purpose:** Concrete design research for the 5 critical-service partitions
> that sit alongside the Linux guest on the WLTIOS seL4 host: Tor Gateway,
> Key Vault, Firewall, Boot Verifier, Auth Service.
>
> **Companion to:** `docs/context/RESEARCH_SEL4_LINUX_VM.md` (which establishes
> that seL4 + Linux-as-guest on x86-64 is technically viable). This document
> assumes the reader has read that document and accepted ADR-02.
>
> **Author:** Researcher agent (WLTIOS)
> **Date:** 2026-06-21
> **Status:** COMPLETE — ready for ADR-03 (partition framework + skeletons)
> **Sources cited inline; full URL list at bottom.**

---

## TL;DR — The Verdict (Read This First)

**The 5 planned WLTIOS partitions are all buildable on seL4 today, but the
deviation from prior work varies by partition.** This is the honest assessment:

| Partition | Buildable today on seL4? | Existing reference | Effort |
|---|---|---|---|
| **Firewall** | ✅ Yes, demonstrated | LionsOS Firewall example (sDDF) [[1]](https://lionsos.org/docs/examples/firewall/) | 🟢 LOW — port and adapt |
| **Boot Verifier** | ✅ Yes, designed | Berridge-Argent 2021 thesis on seL4 + TPM attestation [[2]](https://nickba.dev/thesis/thesis.pdf) | 🟢 LOW |
| **Key Vault** | ✅ Yes, by composition | CAmkES/Microkit PD + cross-VM dataport + static crypto code | 🟡 MEDIUM — write crypto interface |
| **Auth Service** | ✅ Yes, by composition | Same pattern as Key Vault | 🟡 MEDIUM |
| **Tor Gateway** | ⚠️ Partial — *native* Tor daemon in a PD is hard | No prior art on seL4; see §10.1 | 🔴 HIGH — design trade-off required |

**Cross-cutting answers to the user's "specific questions":**

1. **Can a Tor daemon run in a seL4 partition?**
   *Not as-is.* Tor is a POSIX program (~400 KLOC) that assumes a libc, a
   network stack, a scheduler, and a filesystem. seL4 provides none of those
   natively — a Microkit protection domain (PD) is a bare-metal single-threaded
   event-driven program with no libc, no sockets, no files. **Two realistic
   paths:** (a) run Tor inside a *second* tiny Linux guest in its own VM
   (heavyweight, breaks the "no-POSIX-in-critical-services" purity but isolates
   Tor from the main Linux guest), or (b) port Tor to run on a Microkit-hosted
   userspace TCP/IP stack (e.g. sDDF + lwIP — significant porting effort).
   See §10.1 for full analysis.

2. **How does the Linux guest send network packets to the Tor partition?**
   Via a **virtio-net device** emulated by the VMM, backed by **sDDF net
   queues** that hand buffers to the Tor PD. This is exactly the pattern
   LionsOS uses for its Firewall. See §7 + §10.1.

3. **How does the Key Vault encrypt/decrypt files for Linux apps without
   exposing keys?**
   Linux apps send *plaintext bytes* (or plaintext block-aligned chunks)
   through a virtio console / cross-VM dataport to the Key Vault PD; the PD
   holds the keys in its own private memory (which Linux cannot map), performs
   the crypto, and returns the ciphertext (or vice versa). The keys **never
   leave the PD's address space** — Linux has no capability to the PD's
   memory frames. See §10.2.

4. **How does the Firewall partition intercept packets before they reach the
   NIC?**
   It doesn't intercept — it **sits in the data path by construction**. The
   NIC driver PD reads packets from the hardware, writes them to an sDDF
   queue; the Firewall PD dequeues, applies rules, and either drops or
   forwards to the next component (Tor, Linux, etc.). The NIC driver never
   has a direct channel to the Linux guest. This is precisely the LionsOS
   firewall architecture. See §10.3.

**Recommended framework: Microkit** (not CAmkES). The seL4 whitepaper is
unambiguous: *"the Microkit is the recommended framework for building systems
on seL4"* [[3]](https://sel4.systems/About/seL4-whitepaper.pdf). CAmkES is the
older predecessor with a more painful build process and is in the process of
being superseded for new projects (per the seL4 Summit 2024 talk
"Transitioning from CAmkES VMM to Microkit VMM" [[4]](https://sel4.systems/Summit/2024/slides/camkes-to-microkit.pdf)).
Microkit + libvmm is the path forward. See §3 for full comparison.

---

## 1. What is a seL4 "Partition"?

In seL4 terminology, what WLTIOS calls a "partition" is one of:

- A **protection domain (PD)** in Microkit — an isolated address space with
  its own thread, capability space, and scheduling parameters. This is the
  primary meaning.
- A **component instance** in CAmkES — same underlying concept, higher-level
  abstraction.
- A **virtual machine (VM)** — a special kind of PD that runs a guest OS at a
  de-privileged privilege level (VMX non-root on x86, EL1 on ARM).

The defining property of all three is: **the seL4 kernel enforces memory
isolation between them using the hardware MMU, and access rights are
governed by capabilities**. If a PD does not have a capability to another
PD's memory, the kernel's formal proof guarantees it cannot read or write
that memory.

> "A protection domain (PD) is the fundamental runtime abstraction in
> Microkit. It is analogous, but very different in detail, to a process on a
> UNIX system. A PD provides a thread of control that executes within a fixed
> virtual address space. The isolation provided by the virtual address space
> is enforced by the underlying hardware MMU." — Microkit Manual [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

A PD in Microkit has *three or four entry points* (not a `main()`):
- `init()` — called exactly once at boot
- `notified(channel)` — called when another PD signals on a channel
  (asynchronous, semaphore-like)
- `protected(channel, msg)` — called when another PD invokes a protected
  procedure (synchronous, RPC-like)
- `fault(child, msg)` — called when a child PD/VM faults (only for PDs that
  manage children)

This is the **event-driven single-threaded** model. There is no scheduler
inside a PD, no `sleep()`, no preemption within the PD. All activity is
initiated by an interrupt (turned into a `notified` call) or a `ppcall` from
another PD. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md) [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

The seL4 kernel supports up to **63 PDs** in Microkit (per the manual),
plenty for WLTIOS's 5 partitions + VMM + sDDF driver PDs + serial server.

---

## 2. The Two Frameworks (and the third, Genode)

There are three component frameworks officially supported on seL4. The
seL4 whitepaper covers all three in §3.1–3.2: [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

### 2.1 CAmkES (Component Architecture for microKernel-based Embedded Systems)

- The **older** framework, developed by NICTA/Data61/UNSW. [[6]](https://docs.sel4.systems/projects/camkes/manual.html)
- Uses an **Architecture Description Language (ADL)** — a custom DSL with
  C-like syntax — to describe components, interfaces, and connections.
- Has four interface kinds: **procedure** (RPC), **event** (async signal),
  **dataport** (shared memory), and **hardware** (device). [[6]](https://docs.sel4.systems/projects/camkes/manual.html)
- Provides built-in connectors: `seL4RPC`, `seL4Notification`, `seL4SharedData`,
  `seL4VirtQueues`, `seL4Ethdriver`, `seL4VMDTBPassthrough`, etc.
- Components are **multi-threaded** by default (one thread per interface).
- Ships with `libsel4camkes`, which provides a subset of POSIX syscalls,
  memory allocators, and DMA pools.
- **Lacks an SDK** — forces use of the seL4 kernel's complex CMake build
  system.
- Used by `camkes-vm-examples` (the reference VMM examples, including
  `optiplex9020` and `cma34cr_centos` cited in the companion research doc).

### 2.2 Microkit (formerly seL4 Core Platform / seL4CP)

- The **newer** framework (seL4 Foundation-blessed successor), co-developed
  by UNSW Trustworthy Systems and Breakaway Consulting. Initial release
  2022; v2.0.0 in March 2025, v2.2.0 current. [[7]](https://trustworthy.systems/projects/microkit)
- Uses an **XML system description file (SDF)** — declarative, easy to
  diff, no DSL parser needed.
- Provides **exactly four abstractions**: protection domain, memory region,
  channel, and (optional) virtual machine. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)
- PDs are **single-threaded and event-driven** by design (init / notified /
  protected entry points).
- Distributed as a **proper SDK** with `libmicrokit.a`, kernel images, and
  a `microkit` tool that turns the SDF + per-PD ELF files into a bootable
  image. **Build system is your choice** — Make, CMake, Bazel, whatever.
- **Currently uses the MCS kernel configuration** (mixed-criticality
  scheduling), which itself is still undergoing verification (scheduled
  for RISC-V in 2026, AArch64 in 2027). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)
- Forms the basis of **LionsOS** (UNSW's seL4-based general-purpose OS) and
  the **sDDF** driver framework. [[8]](https://lionsos.org) [[9]](https://github.com/sel4-cap/sDDF)

### 2.3 Genode

- A more powerful, general framework that supports multiple microkernels
  (seL4, L4ka::Pistachio, Fiasco.OC, base-hw, Linux).
- Already comes with a wealth of services and device drivers, especially
  on x86.
- **Two drawbacks** per the seL4 whitepaper: (1) because it supports
  weaker microkernels too, it's based on a least-common-denominator API
  and "cannot use all of seL4's security and safety features"; (2) "it has
  no assurance story." [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

### 2.4 The seL4 Foundation's own recommendation

The whitepaper is direct:

> "For these reasons, the Microkit is the recommended framework for building
> systems on seL4, at least as long as the static architecture works."
> — seL4 whitepaper §3.1 [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

The seL4 Summit 2024 talk "Transitioning from CAmkES VMM to MicroKit VMM"
confirms the migration direction: CAmkES VMM is being deprecated in favor
of a Microkit-based VMM (`libvmm`), with sDDF integration, simpler
abstractions, and easier development. [[4]](https://sel4.systems/Summit/2024/slides/camkes-to-microkit.pdf)

### 2.5 Recommendation for WLTIOS

**Use Microkit + libvmm.** Reasons:

1. The seL4 Foundation explicitly recommends it for new projects. [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)
2. CAmkES is being deprecated for VM work — investing in CAmkES now would
   be technical debt that needs to be paid back when camkes-vm-examples
   stops receiving updates.
3. The LionsOS Firewall example (the closest existing analog to WLTIOS's
   Firewall partition) is built on Microkit + sDDF — direct template. [[1]](https://lionsos.org/docs/examples/firewall/)
4. Microkit has a real SDK and lets us use any build system, which fits
   WLTIOS's "custom init, no systemd, Nix-style package manager" philosophy.
5. Microkit uses the MCS kernel, which gives us scheduling contexts and
   passive servers — important for predictable partition behavior.

The one **legitimate** argument *for* CAmkES: the `camkes-vm-examples`
repo has more mature x86-64 VMM examples (`minimal_64`, `optiplex9020`,
`cma34cr_centos`) that already demonstrate cross-VM connectors on x86.
[[10]](https://github.com/seL4/camkes-vm-examples) We will likely borrow
*configuration patterns* from those examples even while building on
Microkit. The CAmkES-to-Microkit transition guide explicitly shows the
1-to-1 mapping. [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf)

---

## 3. CAmkES vs Microkit: Head-to-Head

| Criterion | CAmkES | Microkit |
|---|---|---|
| **Maturity** | Older, more battle-tested in past projects | Newer (2022+), but seL4 Foundation's recommended path |
| **System description** | Custom ADL DSL (.camkes files) | XML system description file |
| **Abstractions** | procedure/event/dataport/hardware + many connectors | PD + memory_region + channel + virtual_machine |
| **Threading model** | Multi-threaded components (one thread per interface) | Single-threaded, event-driven PDs |
| **Build system** | CMake (forced) | Any (Make, CMake, Bazel, Nix...) |
| **SDK** | None — must use kernel build system | Yes — downloadable SDK per board |
| **Kernel config** | Default (mainline) kernel | MCS kernel (mixed-criticality) |
| **Verification status** | Older, less amenable to push-button verification | Designed for push-button verification; Microkit/CapDL proofs in progress [[3]](https://sel4.systems/About/seL4-whitepaper.pdf) |
| **VMM library** | camkes-vm (VM_Arm, VM, libsel4vm, libsel4vmmplatsupport) | libvmm (au-ts/libvmm) [[12]](https://github.com/au-ts/libvmm) |
| **x86-64 VM support** | Demonstrated (cma34cr_centos, minimal_64, optiplex9020) | Experimental (libvmm requires an experimental Microkit SDK build for x86 VM as of v2.2.0) [[12]](https://github.com/au-ts/libvmm) |
| **Network stack** | CAmkES sDDF (deprecated) | sDDF (actively developed, near-wire-speed) [[9]](https://github.com/sel4-cap/sDDF) |
| **Reference OS** | (none — camkes-vm-examples only) | LionsOS [[8]](https://lionsos.org) |
| **Cross-VM connectors** | First-class (`seL4SharedData`, `seL4Notification`, virtqueues, libcrossvm) | Built from primitives (shared memory region + channel) — see §7 |
| **CAmkES attribute system** | Rich (custom attributes → global symbols) | No built-in equivalent; hardcoded in PD source or metaprogram |

### 3.1 The x86-64 VM caveat (important for WLTIOS)

The libvmm README contains this warning:

> "For the x86-64 VM, you will need an experimental version of the Microkit
> SDK. It is based on the main branch of Microkit, but have changes needed
> for the x86 VM that have not made it into a release." [[12]](https://github.com/au-ts/libvmm)

This means: if WLTIOS targets x86-64 (per ADR-02), the Microkit + libvmm
path for the Linux guest is **not yet at the same maturity** as the CAmkES
VMM path on x86-64. This is the single biggest practical risk of choosing
Microkit for WLTIOS.

**Mitigation options:**
1. Use Microkit for the 4 native partitions (Tor, Key Vault, Firewall, Boot
   Verifier, Auth), and use CAmkES for the VMM only. **This requires running
   both frameworks in the same image** — which is unusual but technically
   possible since both compile to seL4 capDL. Not recommended; confusing.
2. Use Microkit + libvmm everywhere, accepting the experimental x86 SDK
   build. This is the future-aligned path; risk is that we hit bugs in
   unreleased code.
3. Use CAmkES everywhere for Phase 1, plan migration to Microkit in Phase 2
   once x86-64 libvmm stabilises. Lowest short-term risk, takes on tech debt.

**Recommendation:** Option 2, with a fallback to Option 3 if we hit
blocking bugs. Start by attempting to boot `libvmm`'s `simple` example on
QEMU x86-64; if that works, we're on Option 2. If it doesn't boot, fall
back to CAmkES VMM for Phase 1.

---

## 4. How Partitions Are Defined (Concrete Examples)

### 4.1 CAmkES ADL example

A minimal CAmkES system with two components and a connector (from the
CAmkES-to-Microkit transition guide [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf)):

```camkes
component Ping {
    control;
    dataport Buf(0x1000) sharedbuf_1;
    dataport Buf(0x1000) sharedbuf_2;
    emits Done n1;
    consumes Ready n2;
}

component Pong {
    control;
    dataport Buf(0x1000) sharedbuf_1;
    dataport Buf(0x1000) sharedbuf_2;
    consumes Done n1;
    emits Ready n2;
}

assembly {
    composition {
        component Ping ping;
        component Pong pong;
        connection seL4SharedData channel1(from ping.sharedbuf_1, to pong.sharedbuf_1);
        connection seL4Notification ntfn1(from ping.n1, to pong.n1);
        connection seL4SharedData channel2(from ping.sharedbuf_2, to pong.sharedbuf_2);
        connection seL4Notification ntfn2(from ping.n2, to pong.n2);
    }
}
```

Terminology (per CAmkES manual [[6]](https://docs.sel4.systems/projects/camkes/manual.html)):
- `control` — the component has a `run()` entry point (an active thread).
- `provides Iface i` — the component *implements* the procedure interface
  `Iface`, callable by other components via an RPC connector.
- `uses Iface i` — the component *calls* the procedure interface `Iface`
  provided elsewhere.
- `emits Event e` — the component produces events of type `e` (async).
- `consumes Event e` — the component handles events of type `e`.
- `dataport Buf b` — the component has a shared-memory region named `b`.
- `connection Connector c(from a.x, to b.y)` — wires up interfaces between
  instances using a named connector.

### 4.2 Microkit SDF example (equivalent)

The same system in Microkit: [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<system>
    <memory_region name="sharedbuf_1" size="0x1_000" />
    <memory_region name="sharedbuf_2" size="0x1_000" />

    <protection_domain name="ping" priority="42">
        <program_image path="ping.elf" />
        <map mr="sharedbuf_1" vaddr="0x3000000" perms="rw" setvar_vaddr="send_buf" />
        <map mr="sharedbuf_2" vaddr="0x3001000" perms="r"  setvar_vaddr="recv_buf" />
    </protection_domain>

    <protection_domain name="pong" priority="42">
        <program_image path="pong.elf" />
        <map mr="sharedbuf_2" vaddr="0x3000000" perms="rw" setvar_vaddr="send_buf" />
        <map mr="sharedbuf_1" vaddr="0x3001000" perms="r"  setvar_vaddr="recv_buf" />
    </protection_domain>

    <channel>
        <end pd="ping" id="0" />
        <end pd="pong" id="0" />
    </channel>
    <channel>
        <end pd="ping" id="1" />
        <end pd="pong" id="1" />
    </channel>
</system>
```

Note the patterns:
- A `<memory_region>` is *physical* memory (can be mapped into multiple
  PDs — that's how shared memory works).
- A `<map>` inside a PD maps a region at a specific **virtual address**
  with specific permissions. The optional `setvar_vaddr` rewrites a symbol
  in the PD's ELF so the code knows where the buffer lives at runtime.
- A `<channel>` connects exactly two PDs by assigning each a local channel
  id. The channel can carry **notifications** (async, bidirectional, no
  data) and/or **protected procedure calls** (sync, unidirectional caller
  → callee, up to 64 words of data). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

### 4.3 Putting a Linux guest + a native partition in the same system

A real WLTIOS-style system would look like (skeleton):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<system>
    <!-- Guest RAM for the Linux VM -->
    <memory_region name="guest_ram" size="0x400_000_00" page_size="0x200_000" />

    <!-- Shared memory for sDDF net queues between VMM, firewall, tor -->
    <memory_region name="fw_rx_avail" size="0x200_000" page_size="0x200_000" />
    <memory_region name="fw_rx_used"  size="0x200_000" page_size="0x200_000" />
    <memory_region name="fw_tx_avail" size="0x200_000" page_size="0x200_000" />
    <memory_region name="fw_tx_used"  size="0x200_000" page_size="0x200_000" />
    <memory_region name="fw_data"     size="0x800_000" page_size="0x200_000" />

    <!-- NIC MMIO for the driver PD -->
    <memory_region name="nic_regs" size="0x10_000" phys_addr="0xfb000000" />

    <!-- Tor ↔ Key Vault shared memory -->
    <memory_region name="kv_request_buf" size="0x1000" />

    <!-- 1. NIC driver PD (highest priority, passive, runs only on IRQ) -->
    <protection_domain name="nic_driver" priority="200" passive="true">
        <program_image path="nic_driver.elf" />
        <map mr="nic_regs" vaddr="0x50000000" perms="rw" cached="false" />
        <map mr="fw_rx_avail" vaddr="0x60000000" perms="rw" setvar_vaddr="rx_avail" />
        <map mr="fw_rx_used"  vaddr="0x61000000" perms="rw" setvar_vaddr="rx_used" />
        <map mr="fw_data"     vaddr="0x62000000" perms="rw" setvar_vaddr="dma_pool" />
        <irq pin="20" vector="40" id="0" ioapic="0" trigger="edge" />
    </protection_domain>

    <!-- 2. Firewall PD -->
    <protection_domain name="firewall" priority="150">
        <program_image path="firewall.elf" />
        <map mr="fw_rx_avail" vaddr="0x60000000" perms="r"  setvar_vaddr="rx_avail" />
        <map mr="fw_rx_used"  vaddr="0x61000000" perms="rw" setvar_vaddr="rx_used" />
        <map mr="fw_tx_avail" vaddr="0x64000000" perms="rw" setvar_vaddr="tx_avail" />
        <map mr="fw_tx_used"  vaddr="0x65000000" perms="r"  setvar_vaddr="tx_used" />
        <map mr="fw_data"     vaddr="0x62000000" perms="rw" setvar_vaddr="data" />
    </protection_domain>

    <!-- 3. Tor PD -->
    <protection_domain name="tor" priority="100">
        <program_image path="tor.elf" />
        <!-- maps to other shared regions... -->
    </protection_domain>

    <!-- 4. Key Vault PD (high priority - critical) -->
    <protection_domain name="key_vault" priority="200" passive="true" pp="true">
        <program_image path="key_vault.elf" />
        <map mr="kv_request_buf" vaddr="0x70000000" perms="rw" setvar_vaddr="req_buf" />
    </protection_domain>

    <!-- 5. VMM PD (lowest priority - runs Linux guest) -->
    <protection_domain name="vmm" priority="50">
        <program_image path="vmm.elf" />
        <map mr="guest_ram" vaddr="0x80000000" perms="rw" setvar_vaddr="guest_ram_vaddr" />
        <virtual_machine name="linux">
            <vcpu id="0" />
            <map mr="guest_ram" vaddr="0" perms="rwx" />
        </virtual_machine>
        <ioport id="0" addr="0x3f8" size="8" /> <!-- COM1 for serial -->
        <irq pin="4" vector="41" id="1" ioapic="0" trigger="edge" />
    </protection_domain>

    <!-- Channels: who can talk to whom -->
    <channel>
        <end pd="nic_driver" id="0" />
        <end pd="firewall"   id="0" />
    </channel>
    <channel>
        <end pd="firewall"   id="1" />
        <end pd="tor"        id="0" />
    </channel>
    <channel>
        <end pd="firewall"   id="2" />
        <end pd="vmm"        id="0" />
    </channel>
    <channel>
        <end pd="tor"        id="1" />
        <end pd="vmm"        id="1" />
    </channel>
    <channel>
        <end pd="vmm"        id="2" />
        <end pd="key_vault"  id="0" pp="true" />
    </channel>
</system>
```

This SDF is the **single source of truth** for who-can-talk-to-whom. The
Microkit promise is: *"what is specified in the SDF is a faithful
representation of the possible interactions. In particular, it promises
that no interactions are possible beyond those deﬁned in the SDF."*
[[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

So if `tor` and `key_vault` don't share a channel, **Tor literally cannot
talk to the Key Vault**, even if both are compromised.

### 4.4 The `capDL` underneath

Beneath the SDF, Microkit translates to **capDL** (Capability Distribution
Language), the lower-level seL4 system description that lists every kernel
object and every capability in the system. [[13]](https://docs.sel4.systems/projects/capdl) [[14]](https://docs.sel4.systems/projects/capdl/lang-spec.html)

The system designer normally doesn't write capDL by hand — the Microkit
tool generates it from the SDF. But it's worth knowing it exists because:

1. **Auditing**: To prove that PD A cannot access PD B's memory, you read
   the generated capDL file and confirm B's frames aren't in A's CSpace.
2. **Verification**: The Microkit tool itself is being verified to produce
   capDL that faithfully implements the SDF (in progress as of 2025).
3. **Custom advanced setups**: You can drop down to raw capDL if you need
   something Microkit doesn't expose (the optional `<cspace>` SDF element
   lets you add extra capabilities). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

---

## 5. IPC Between Partitions

There are **three** mechanisms for inter-partition communication in
Microkit (the same three primitives exist in CAmkES under different
names). seL4's "IPC" name is misleading — read this section carefully.

### 5.1 What seL4 "IPC" actually is (and isn't)

Gernot Heiser (the seL4 lead) has a famous blog post titled *"How to (and
how not to) use seL4 IPC"*. The key points: [[15]](https://microkerneldude.org/2019/03/07/how-to-and-how-not-to-use-sel4-ipc/)

> "IPC is a message passing mechanism, right? After all, it's an acronym
> for inter-process communication? That's historically true, but seL4 IPC
> has come a long way, and its IPC primitive is quite a different beast
> from message-passing primitives offered by other OSes."

> "seL4 IPC is **not** a mechanism for shipping data.
> seL4 IPC is **not** a mechanism for synchronising activities."

> "IPC is a user-controlled context switch with benefits. You switch to a
> different thread (usually in a different address space), without
> involving the scheduler. The benefit is that you get to carry along a
> bit of data."

> "IPC is the seL4 mechanism for implementing cross-domain function
> calls."

The "no-no's" of seL4 IPC, per Heiser:
- Don't use it for bulk data transfer (use shared memory + notifications).
- Don't use send-only or receive-only IPC for normal traffic (only for
  protocol init / exception handling).
- Don't use IPC for synchronisation — use **Notification objects**.
- Don't use cross-core IPC (use shared memory + notifications).

**Implication for WLTIOS:** The Tor, Firewall, and Key Vault partitions
will use **shared memory + notifications** for their data paths. They
will use **protected procedure calls (ppcall)** only for short control
messages (e.g. "key_vault.encrypt(chunk_id=42)").

### 5.2 Mechanism 1: Notifications (async, semaphore-like)

A **notification** is a binary semaphore that signals across PD
boundaries. It carries no data — just the channel identity. Use it to
say "data is ready in the shared buffer; come look."

```c
// In PD "firewall", after writing to a shared buffer:
microkit_notify(1);  // wake up the Tor PD on channel 1
```

```c
// In PD "tor":
void notified(microkit_channel ch) {
    switch (ch) {
        case 0: /* NIC has a packet for us */ break;
        case 1: /* Firewall says data ready */ break;
    }
}
```

Notifications are **async and non-blocking** — the sender never waits. If
the receiver is busy, multiple notifications may coalesce into one (since
it's binary). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md) [[16]](https://docs.sel4.systems/projects/microkit/tutorial/part2.html)

### 5.3 Mechanism 2: Protected Procedure Calls (sync, RPC-like)

A **protected procedure call (PPC)** is a synchronous function call across
PD boundaries. The caller blocks until the callee returns. Up to 64
machine words of arguments can be passed via "message registers". [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md) [[17]](https://docs.sel4.systems/projects/microkit/tutorial/part3.html)

```c
// In PD "vmm" (Linux guest) — ask the key vault to encrypt:
microkit_msginfo msg = microkit_msginfo_new(OP_ENCRYPT, 2);
microkit_mr_set(0, chunk_id);
microkit_mr_set(1, chunk_len);
microkit_msginfo reply = microkit_ppcall(KEY_VAULT_CH, msg);
// reply contains status code
```

```c
// In PD "key_vault":
microkit_msginfo protected(microkit_channel ch, microkit_msginfo msg) {
    uint64_t op = microkit_msginfo_get_label(msg);
    uint64_t chunk_id = microkit_mr_get(0);
    uint64_t chunk_len = microkit_mr_get(1);
    // ... do crypto on shared buffer chunk_id ...
    return microkit_msginfo_new(STATUS_OK, 0);
}
```

**Critical constraints**:
- A PPC is only allowed from a **lower-priority PD to a higher-priority PD**
  (prevents deadlocks).
- The call graph between PDs must form a **directed, acyclic graph** — no
  cycles, even transitively. So `A → B → A` is forbidden.
- PPCs **block the caller** until the callee returns. The callee runs on
  the caller's scheduling context (CPU time is charged to the caller).
  This is the "passive server" pattern. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)
- The Microkit manual says **PPCs must execute in bounded time**. Future
  versions will enforce this with static analysis. For now, the caller
  must trust the callee. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

**WLTIOS implication**: The Key Vault is a perfect passive server. Linux
makes a PPC to it; the Key Vault does bounded-time crypto; returns.
Keys never leave the Key Vault's address space.

### 5.4 Mechanism 3: Shared memory (the workhorse)

This is the main data-transfer mechanism in any seL4 system. A
`<memory_region>` is mapped into two (or more) PDs; they read/write the
same physical pages directly. No kernel involvement per access — it's
just memory.

The pattern, repeated everywhere in sDDF and LionsOS:
1. PD A writes data into the shared buffer.
2. PD A updates a metadata queue (also shared memory) describing what it
   wrote.
3. PD A calls `microkit_notify(ch)` — sends a signal.
4. PD B's `notified()` entry point wakes up, reads the queue, processes
   the data, optionally writes a response into a *different* shared buffer.
5. PD B notifies PD A.

The **sDDF** formalizes this as **lock-free single-producer
single-consumer queues** with a model-checked signalling protocol. LionsOS
uses it everywhere. [[8]](https://lionsos.org) [[9]](https://github.com/sel4-cap/sDDF)

From the LionsOS docs:

> "Components are connected by lock-free queues using an efficient
> model-checked signalling mechanism. As far as is practical, operating
> systems components do a single thing. Drivers for instance exist solely
> to convert between a hardware interface and a set of queues to talk to
> the rest of the system." [[8]](https://lionsos.org)

This is the **exact pattern** WLTIOS's partitions should follow.

---

## 6. Memory Sharing in Detail

### 6.1 Three flavors of shared memory

| Flavor | How | Use case |
|---|---|---|
| **Normal shared region** | `<memory_region>` mapped into ≥2 PDs with `perms="rw"` | Bulk data: packet buffers, crypto chunks |
| **Device MMIO** | `<memory_region phys_addr="0x...">` with `cached="false"` mapped into 1 PD | Hardware registers (NIC, UART) |
| **DMA pool** | Like above but the physical address is what the device sees | Packets DMA'd by NIC |

The `cached="false"` attribute is critical for MMIO — the CPU must not
cache device-register writes. [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf) [[12]](https://github.com/au-ts/libvmm)

### 6.2 The `setvar_vaddr` trick

A subtle but important pattern: the SDF can rewrite a symbol in the PD's
ELF to the virtual address where a memory region is mapped. So the C
code can say:

```c
extern volatile uint8_t *rx_avail;
// ... use rx_avail as a pointer ...
```

…and at build time the Microkit tool patches the `rx_avail` symbol in the
ELF to be `0x60000000` (or wherever). This avoids hardcoding addresses
and lets the system designer re-layout memory in the SDF without
recompiling the PD code. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md) [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf)

### 6.3 Permissions: read-only vs read-write

The SDF `perms` attribute can be `r`, `w`, `x`, or combinations (no
write-only on ARM). **This is critical for the Key Vault design**: map
the request buffer as `rw` from Linux's side, but the *keys* themselves
live in a memory region that is mapped *only* into the Key Vault PD.

If Linux's CSpace doesn't contain a capability to the Key Vault's
private frames, **Linux literally cannot read those frames** — the seL4
kernel will fault on the access. This is the formal guarantee.

### 6.4 DMA and the IOMMU

For DMA-capable devices (NIC, NVMe, GPU), seL4 programs the Intel VT-d
IOMMU on x86 (or SMMU on ARM) so a device can only DMA into frames
explicitly assigned to it. This is what makes passthrough safe — the NIC
can't DMA into the Key Vault's memory even if a malicious Linux guest
reprograms the NIC's DMA engine.

The libvmm manual notes that on x86-64, guest RAM starts at physical
address 0, which complicates DMA passthrough — this is being worked on
via IOMMU support. [[12]](https://github.com/au-ts/libvmm) On ARM it
works today by aligning the guest RAM `phys_addr` with what the guest
DTS expects.

---

## 7. Linux Guest ↔ Partition Communication (Cross-VM Connectors)

This is the mechanism by which a process running **inside the Linux
guest** talks to a native seL4 PD. There are two approaches:

### 7.1 CAmkES cross-VM connectors (the mature path)

The CAmkES VMM ships with `libcrossvm` and three Linux kernel modules
that get installed in the guest: [[18]](https://docs.sel4.systems/Tutorials/camkes-vm-crossvm.html)

- **`dataport`** — allocates a shared-memory buffer in the guest and
  hypercalls the VMM to set up the shared mapping. Linux processes
  `mmap()` the file associated with the dataport.
- **`emits_event`** — lets a guest process emit an event to a CAmkES
  component via `ioctl()` on a file descriptor.
- **`consumes_event`** — lets a guest process wait or poll for an event
  from a CAmkES component via `poll()` on a file descriptor. The VMM
  injects a virtual interrupt to wake the blocked process.

This is the mechanism the `optiplex9020` example uses for its
`string_reverse` demo (the one cited in the companion research doc).

### 7.2 Microkit / libvmm cross-VM path (the new path)

Microkit doesn't have a built-in "cross-VM connector" abstraction. The
explicit design is: **build it from primitives**. From the CAmkES-to-Microkit
transition guide: [[11]](https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf)

> "Unlike CAmkES components, [PDs are] single-threaded and event-driven to
> keep the programming model and implementations simple."

The cross-VM pattern in Microkit looks like:

1. A `<memory_region>` is mapped into both the VMM PD and the native PD
   (e.g. Key Vault).
2. The VMM **also** maps that region into the guest VM's physical address
   space via a `<map>` inside the `<virtual_machine>` element.
3. Inside the Linux guest, a custom kernel driver (or virtio device)
   discovers the mapping and exposes it to userspace via a character
   device or mmap.
4. To signal, the Linux guest writes to a virtio-console port, or
   triggers a hypercall that the VMM catches and forwards as a
   `microkit_notify()` to the target PD.

In practice, the cleanest path is to use **virtio devices** emulated by
the VMM as the Linux-side interface, with the VMM forwarding traffic
to/from the native PD via shared memory. libvmm already implements
virtio-net, virtio-block, virtio-console, virtio-sound, all using sDDF
queues on the backend. [[12]](https://github.com/au-ts/libvmm)

> "libvmm provides a number of virtIO devices to enable guests to interact
> with the outside world… For each of these devices, libvmm will perform
> I/O using the protocols and interfaces provided by the [sDDF]. This
> allows libvmm to interact with the outside world in a standard way just
> like any other native client program. This means that these virtIO
> device implementations are generic and are not dependent on the platform
> or architecture that libvmm is being used on." — libvmm manual [[12]](https://github.com/au-ts/libvmm)

So **the pattern is**: Linux guest loads `virtio_net` driver → VMM
emulates the virtio-net PCI device → VMM's virtio-net backend puts
packets on an sDDF net queue → the sDDF net virtualiser multiplexes
them to native PDs (Firewall, Tor). From Linux's perspective it's just
a virtio NIC; from the native PDs' perspective it's just an sDDF queue.

### 7.3 Recommendation for WLTIOS

- **Firewall/Tor packet path**: virtio-net in the Linux guest ↔ VMM ↔
  sDDF net queue ↔ Firewall PD ↔ (allow) ↔ Tor PD ↔ sDDF net queue ↔
  NIC driver PD ↔ real NIC. This is the LionsOS pattern. [[1]](https://lionsos.org/docs/examples/firewall/)
- **Key Vault request path**: virtio-console in the Linux guest ↔ VMM
  parses "encrypt chunk N" command ↔ VMM `ppcall`s Key Vault PD. The
  data being encrypted lives in a shared memory region mapped into
  both Linux and Key Vault.
- **Auth request path**: same pattern as Key Vault.
- **Boot Verifier**: no Linux-side communication needed; it runs at
  boot, measures the system, and exposes attestation data via a
  read-only shared page if needed.

---

## 8. Building Partitions

### 8.1 The Microkit SDK workflow

1. **Download** the Microkit SDK for your host OS + target board from
   the Microkit releases page (e.g. v2.2.0). [[7]](https://trustworthy.systems/projects/microkit)
   The SDK is a tarball containing:
   - `bin/microkit` (the system-builder tool)
   - `libmicrokit.a` per board/config
   - seL4 kernel images per board/config
   - `initialiser.elf`, `monitor.elf` (boot infrastructure)
   - Examples in `example/`

2. **Write your PD programs.** Each PD is a separate ELF, compiled and
   linked against `libmicrokit.a`. You can use Make, CMake, Bazel,
   Nix — Microkit doesn't care. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

3. **Write the SDF** (the XML system description).

4. **Run the Microkit tool**:
   ```bash
   microkit --board x86_64_generic --config release \
            --search-path build/ system.system
   ```
   This produces `loader.img` (the bootable image) and `report.txt`
   (diagnostics). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

5. **Boot the image** on QEMU or real hardware. On x86-64 the image is
   an ELF loaded as a Multiboot 2 module (e.g. by GRUB). On ARM/RISC-V
   it's loaded by U-Boot via `go` or `bootelf`.

### 8.2 Language support

- **C/C++**: first-class, via `libmicrokit.a`.
- **Rust**: bindings at [rust-sel4](https://github.com/seL4/rust-sel4);
  Colias Group has a Rust-on-Microkit training. [[19]](https://coliasgroup.com/seL4-summit-2024-rust-training/rustdoc/microkit/aarch64-sel4-microkit/doc/sel4_microkit/index.html)
- **No Python, no POSIX**: a PD has no libc unless you bring one.
  LionsOS has a "libc" page discussing integration options. [[8]](https://lionsos.org)

This is the single biggest practical gotcha for WLTIOS — see §10.1 on
the Tor partition.

### 8.3 Boards supported by Microkit (relevant subset)

From the manual's BSP chapter: [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

- `x86_64_generic` — generic x86-64 PC (no VT-x)
- `x86_64_generic_vtx` — generic x86-64 PC with VT-x (this is what WLTIOS needs for VMs)
- `qemu_virt_aarch64` — QEMU ARM virt (most common test platform)
- Various ARM boards: imx8mm_evk, imx8mp_evk, rockpro64, rpi4b_*, maaxboard, ultra96v2, etc.
- RISC-V: ariane, hifive_p550, qemu_virt_riscv64, star64.

For WLTIOS Phase 1 we target `x86_64_generic_vtx` (QEMU + Kushal's
i5-1135G7 laptop). The companion research doc covers the QEMU
invocation. [[20]](https://docs.sel4.systems/projects/microkit/tutorial/part4.html)

### 8.4 Build system choice for WLTIOS

Given the project's "Nix-style package manager, atomic, reproducible"
philosophy (per MASTER_CONTEXT §1), the natural choice is **Nix flakes**
for the build system, calling the Microkit SDK as a Nix derivation. This
would give us:
- Reproducible builds (anyone can rebuild and get the same image).
- Easy pinning of Microkit SDK / kernel versions.
- Per-PD dependencies tracked explicitly.

Alternative: plain Makefiles (what libvmm examples use). Simpler to
start, harder to reproduce later.

---

## 9. Security Considerations

### 9.1 What seL4's proofs actually guarantee

From the seL4 whitepaper and proofs page: [[3]](https://sel4.systems/About/seL4-whitepaper.pdf) [[21]](https://sel4.systems/Verification/proofs.html)

The verified properties of the seL4 kernel:
1. **Functional correctness** (2009): the C implementation is a correct
   refinement of the formal specification.
2. **Integrity** (2011): the kernel prevents a subject from modifying
   data without explicit authorisation.
3. **Confidentiality** (2013): the separation-kernel configuration
   enforces information-flow security — *unauthorised reads fail*. This
   includes covert storage channels.
4. **Binary correctness** (2013): the compiled binary is a correct
   translation of the verified C code (translation validation). Removes
   trust in gcc/Clang.
5. **WCET analysis** (2011, improved since): the kernel's worst-case
   execution time is bounded, making it suitable for hard real-time.

### 9.2 What the proofs do NOT cover

This is critical for WLTIOS's threat model:

- **Timing channels** are NOT covered. The formalism has no notion of
  time, so it cannot prove timing-channel prevention. This is an active
  research area. [[15]](https://microkerneldude.org/2019/03/07/how-to-and-how-not-to-use-sel4-ipc/) [[22]](https://sel4.systems/About/FAQ.html)
  → Implication: a malicious Linux guest could in principle leak
  secrets from the Key Vault through cache timing. Mitigation: cache
  colouring (research-grade, not in mainline seL4 yet) or accepting
  this risk for v1.
- **The MCS kernel config used by Microkit is still being verified**
  (RISC-V scheduled for 2026, AArch64 for 2027). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)
  → Implication: if WLTIOS uses Microkit, we're using a kernel config
  that's not yet formally proven. The design proofs are done; the
  code-conformance proofs are in progress.
- **The VMM is not verified** (already covered in the companion doc and
  ADR-02).
- **Hardware attacks** (DMA beyond IOMMU, JTAG, evil maid) are out of
  scope. [[2]](https://nickba.dev/thesis/thesis.pdf)
- **The Microkit tool itself** is being verified to produce correct
  capDL, but this proof is in progress. [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)
- **The boot code** is not yet verified (the proofs assume "if the
  kernel boots into a safe state, it remains in one"). [[23]](https://microkerneldude.org/2019/08/06/10-years-sel4-still-the-best-still-getting-better/)

### 9.3 Isolation verification techniques

How to actually confirm your partitions are isolated:

1. **Read the SDF**. The Microkit promise: no interactions are possible
   beyond those defined in the SDF. [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)
   If `tor` and `key_vault` don't share a channel, they cannot
   communicate. Audit by inspection.
2. **Read the generated capDL**. The Microkit tool can emit the capDL
   for your system; from there, you can mechanically check that PD A's
   CSpace doesn't contain capabilities to PD B's private frames. This
   is the formal guarantee. [[13]](https://docs.sel4.systems/projects/capdl) [[14]](https://docs.sel4.systems/projects/capdl/lang-spec.html)
3. **Fault injection**. From inside the Linux guest, try to read
   physical memory that belongs to a partition (e.g. via `/dev/mem` or
   `devmem`). With VT-d and proper guest memory capping, these accesses
   will fault and be caught by the VMM. This proves VM-to-PD isolation.
   (Already covered in companion doc §5.4.)
4. **The Microkit report**. The `microkit` tool emits a `report.txt`
   alongside the image, listing all objects and channels. Review it
   before each release.
5. **Static analysis of PD code**. Each PD's code should be small
   enough to audit by hand or with static analyzers (the LionsOS
   firewall PD is ~1-2 KLOC; the Key Vault would be similar).

### 9.4 Common mistakes (from the Microkit manual and LionsOS docs)

- **Mapping the same `memory_region` RW into two PDs when one should be
  RO.** Always use the least privilege. The Key Vault's private key
  memory should not be mapped anywhere except the Key Vault PD.
- **Using PPC for bulk data.** Per Heiser, PPCs are for control, not
  data. [[15]](https://microkerneldude.org/2019/03/07/how-to-and-how-not-to-use-sel4-ipc/)
- **Letting the VMM have capabilities to partition memory.** The VMM
  should only have capabilities to the guest's RAM and the virtio
  backend queues — not to the Key Vault's private frames. Otherwise a
  VMM compromise = Key Vault compromise.
- **Passing real interrupt numbers in plaintext config.** The Microkit
  manual notes this is fine because the SDF is a build-time artifact,
  but make sure your build pipeline doesn't leak it.
- **Forgetting `cached="false"` for MMIO.** Will cause weird
  heisenbugs. [[12]](https://github.com/au-ts/libvmm)
- **DMA from a passthrough device into the wrong frames.** Mitigated by
  VT-d / SMMU, but only if seL4 is correctly configured. The cma34cr
  example shows the right pattern.
- **Trusting the guest OS.** LionsOS puts it bluntly:
  > "We restrict each VMM to only service exactly one guest as if the
  > guest compromises the VMM then it is still isolated from the rest
  > of the system." [[8]](https://lionsos.org)
  This is why the architecture diagram in §4.3 has the VMM as a peer of
  the Firewall, not as a parent of it.

### 9.5 The Trusted Computing Base (TCB) for each WLTIOS partition

The TCB is "everything that has to be correct for the partition's
security property to hold." For the Key Vault partition, the TCB is:

- The seL4 kernel (verified for ARMv7/v8 and RISC-V; in progress for x86-64
  and for the MCS config).
- The Microkit tool and its capDL generator (verification in progress).
- The Microkit initialiser (the boot-time setup code; not yet verified).
- The Key Vault PD's own code (small, auditable).
- The crypto library used inside the Key Vault (e.g. a minimal
  Curve25519+AES-GCM implementation, or a vetted library like
  libsodium's core primitives).
- The hardware (CPU, MMU, IOMMU).

Critically, the TCB **does not include**:
- The Linux guest kernel.
- The VMM (if VMM is compromised, it cannot read the Key Vault's frames
  because they're not in its CSpace).
- The Tor partition (if Tor is compromised, it has no channel to the
  Key Vault, so cannot request keys).
- The Firewall partition (same).

This is the **load-bearing security property** of the WLTIOS
architecture. It's what differentiates WLTIOS from Tails (where a
Linux kernel compromise exposes everything).

---

## 10. Specific Partition Designs

### 10.1 Tor Gateway Partition

**The hard one.** No prior art on seL4.

#### 10.1.1 The problem

Tor (the onion router daemon) is a userspace POSIX program (~400 KLOC of C).
It expects:
- A POSIX-like environment with `epoll`, `socket`, `read`, `write`,
  `gettimeofday`, `pthread`, etc.
- A TCP/IP stack (Berkeley sockets API).
- A filesystem (it reads `torrc`, writes state files, caches directory
  info).
- A clock (for circuit timing, replay protection).
- Sources of entropy.

A Microkit PD provides **none** of these natively. A PD is a bare-metal
single-threaded event-driven program with no libc. [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)

#### 10.1.2 Three realistic options

**Option A: Port Tor (or a Tor-like proxy) to Microkit natively.**
- Build a minimal libc on top of libmicrokit.
- Use sDDF's network stack (or write a small one).
- Port the parts of Tor we actually need (the client-side proxy: SOCKS5
  listener, circuit construction, onion encryption, cell relay).
- Pros: pure partition, smallest TCB.
- Cons: enormous engineering effort. Likely 6-12 months of work. Tor
  is a moving target. We'd be the first to do it.
- Verdict: **too risky for v1**.

**Option B: Run Tor inside a *second* minimal Linux guest VM.**
- The Linux guest is split into two: the main "WLTIOS userspace" VM
  and a small "Tor VM" running just Tor + a minimal distro (Alpine?).
- The Tor VM is isolated from the main Linux VM by seL4.
- The Tor VM gets network access via the Firewall PD (which is a
  native PD).
- Keys (Tor identity keys) live in the Key Vault PD; the Tor VM
  requests crypto operations via a cross-VM channel.
- Pros: Tor runs unmodified. Qubes OS uses essentially this pattern.
- Cons: defeats some of the "no-POSIX-in-critical-services" purity.
  Adds another VM (~64-128 MB RAM). Boot time goes up.
- Verdict: **pragmatic choice for v1**. Accept the trade-off, document
  it in an ADR.

**Option C: Run Tor natively in the Linux guest, but route its traffic
through the native Firewall PD via virtio-net.**
- This is just Tails's design with a native firewall. The Tor process
  is in the Linux guest; if Linux is compromised, Tor's identity keys
  are exposed.
- Pros: simplest. No partition for Tor at all.
- Cons: defeats the entire purpose of WLTIOS's "Tor in seL4 partition"
  design goal.
- Verdict: **rejected** — contradicts ADR-02.

#### 10.1.3 Recommendation: Option B for v1, prototype Option A for v2

Use Option B (Tor VM) for the v1 architecture. The Tor VM is a
*second* Linux guest (or a buildroot-based minimal Linux) that runs
only Tor and a small SOCKS5 forwarder. Its identity keys live in the
Key Vault PD, accessed via cross-VM virtio-console. Its network
traffic goes through the native Firewall PD via sDDF net queues.

This gives us:
- Tor's process state isolated from the main Linux guest (Qubes-style).
- Tor's identity keys isolated from Tor itself (Key Vault PD holds them).
- Tor's network traffic mediated by a native Firewall (kill switch is
  trivial — Firewall just stops forwarding).
- A clear migration path: if a future version of WLTIOS ports Tor to
  Microkit natively (Option A), the SDF changes but the system
  architecture doesn't.

#### 10.1.4 How packets flow (the user's specific question #2)

```
Linux guest (apps with SOCKS5 client)
    │
    │ (virtio-net: Linux loads virtio_net driver, sees "eth0")
    ▼
VMM PD (libvmm virtio-net backend)
    │
    │ (puts packet on sDDF net TX queue)
    ▼
Firewall PD (applies rules: allow if SOCKS5 to Tor, drop everything else)
    │
    │ (if allowed: forwards to Tor's RX queue)
    ▼
Tor VM (via its own VMM PD; Tor receives via its virtio-net)
    │
    │ (Tor does onion routing; emits encrypted cell)
    ▼
Tor VM TX queue → Firewall PD → sDDF net TX queue → NIC driver PD → NIC
```

This is essentially the LionsOS firewall architecture with an extra
"Tor VM" hop. [[1]](https://lionsos.org/docs/examples/firewall/)

#### 10.1.5 The Key Vault integration

Tor's long-term identity key is the most sensitive secret in the
system. It must not live in the Tor VM (which has Linux and therefore
a kernel attack surface). Instead:

- The Key Vault PD holds the identity key in its private memory.
- Tor VM connects to the Key Vault via virtio-console.
- When Tor needs to sign something with its identity key, it sends
  the data to be signed over the console channel; Key Vault PD
  signs and returns the signature. The key itself never leaves the
  Key Vault.

This is the same pattern as a Hardware Security Module (HSM) — the
WLTIOS Key Vault PD *is* a software HSM, running on verified
isolation instead of dedicated hardware.

### 10.2 Key Vault Partition

**Design pattern: passive server PD with crypto primitives.**

#### 10.2.1 Architecture

```
<virtual_machine> Linux guest ─── virtio-console ─── VMM PD
                                                     │
                                                     │ ppcall (ch 2)
                                                     ▼
                                              Key Vault PD
                                              (priority 200, passive, pp=true)
                                              ├─ holds master key (private memory)
                                              ├─ holds per-file encryption keys
                                              └─ performs encrypt/decrypt/sign/verify
```

#### 10.2.2 Operations (the user's specific question #3)

A Linux app wants to encrypt a file. The flow:

1. **Linux app** opens the file and reads it into a buffer in its
   own memory.
2. **Linux app** makes an `ioctl()` to `/dev/keyvault` (a custom
   kernel module installed in the Linux guest). The ioctl includes
   a file descriptor pointing to the data buffer.
3. The **`/dev/keyvault` driver** (running inside the Linux guest)
   finds the physical page(s) backing the buffer, hypercalls the
   VMM with the physical addresses.
4. The **VMM** maps those pages into a shared memory region visible
   to the Key Vault PD (or — more simply — Linux writes the data
   into a pre-allocated shared memory region by `mmap`ing a
   dataport file, à la CAmkES cross-VM connectors).
5. The **VMM** issues a `microkit_ppcall(KEY_VAULT_CH, msg)` with
   `msg.label = OP_ENCRYPT` and message registers containing
   `[chunk_id, chunk_len]`.
6. The **Key Vault PD**'s `protected()` entry point fires. It looks
   up the chunk in the shared buffer, looks up the appropriate key
   in its *private* memory, performs the encryption in-place (or
   into a different shared buffer for the ciphertext), and returns
   `STATUS_OK`.
7. The **VMM** returns the ioctl result to the Linux app.
8. The **Linux app** writes the ciphertext out to disk.

**Critical property**: at no point does the Linux guest (or the VMM)
have a capability to the Key Vault's key frames. The keys live in
memory that is mapped *only* into the Key Vault PD. Linux cannot
read them even with a kernel exploit, because seL4 itself (verified)
will fault on the access.

#### 10.2.3 What crypto to support

Start minimal:
- `OP_ENCRYPT(chunk_id, chunk_len, key_id)` — AES-GCM with a key
  selected by `key_id`. Returns ciphertext+tag in same chunk.
- `OP_DECRYPT(chunk_id, chunk_len, key_id)` — inverse.
- `OP_SIGN(chunk_id, chunk_len, key_id)` — Ed25519 signature.
- `OP_VERIFY(chunk_id, chunk_len, sig_id, key_id)` — Ed25519 verify.
- `OP_DERIVE_KEY(master_id, label)` — HKDF-SHA256 to derive a
  per-file subkey without exposing the master.

Use a small vetted crypto library — options:
- **libsodium** (core primitives only, ~20 KLOC). Most vetted.
- **monocypher** (~2 KLOC, public domain). Easiest to audit.
- **micro-ecc** (just ECC, ~2 KLOC).

Recommendation: monocypher for v1 (easy to audit by hand, supports
the operations we need). Can swap to libsodium later if we need
more primitives.

#### 10.2.4 Key storage

Keys live in the Key Vault PD's private memory. They are derived at
boot from:
- A master key sealed by the TPM (see Boot Verifier, §10.4).
- Or, for v1 simplicity, a hardcoded key in the PD's ELF — *this is
  not secure against someone with physical access to the disk*, but
  it lets us build the architecture. Upgrade to TPM-sealed keys in v2.

The Key Vault PD is **passive** — it has no scheduling context of
its own after init, so it only runs when called by Linux (or another
PD) via ppcall. This means its CPU usage is charged to the caller.

### 10.3 Firewall Partition (and the kill switch)

**Design pattern: in-line filter PD between NIC driver and the rest
of the system.**

#### 10.3.1 Architecture

We have a *complete reference implementation* to copy from: the
LionsOS Firewall example. From its docs: [[1]](https://lionsos.org/docs/examples/firewall/)

> "The LionsOS project contains an example system that acts as a
> firewall between networks. Each network interface of the firewall
> system has its own instance of an sDDF net subsystem. The firewall
> multiplexes incoming traffic based on its protocol, and permits or
> denies the traffic based on a set of build and run-time
> configurable rules."

The LionsOS firewall has these components (each a separate Microkit
PD):

- **NIC driver PDs** (one per physical NIC) — talk to hardware, put
  packets on sDDF net queues.
- **Rx virtualiser PDs** — multiplex received packets to filters by
  protocol.
- **Filter PDs** (one per protocol: TCP, UDP, ICMP) — apply rules,
  forward or drop.
- **Router PD** — figures out next-hop, updates Ethernet headers,
  hands to TX virtualiser.
- **ARP requester/responder PDs** — maintain MAC tables.
- **ICMP module PD** — generates pings, destination-unreachable.
- **Webserver PD** (Micropython) — runtime rule config via HTTP.

#### 10.3.2 How packets get intercepted (the user's question #4)

The Firewall PD **doesn't intercept packets** — it sits *in the data
path by construction*. The NIC driver PD is the only thing that
talks to the hardware. The NIC driver puts packets on an sDDF queue
that goes to the Firewall PD. The Firewall PD either drops or
forwards to the next PD (Linux VMM, Tor VMM, etc.).

```
   Physical NIC
       │
       │ IRQ → seL4 → notify nic_driver PD
       ▼
   nic_driver PD (reads NIC ring buffer, DMA's packet into shared pool)
       │
       │ (enqueues packet handle on sDDF RX queue, notifies rx_virt)
       ▼
   rx_virt PD (demuxes by protocol)
       │
       │ (enqueues on filter's queue, notifies filter)
       ▼
   firewall PD (applies rules)
       │
       ├─ DROP: return buffer to nic_driver's free pool
       ├─ ALLOW to Linux: enqueue on Linux VMM's RX queue, notify VMM
       └─ ALLOW to Tor: enqueue on Tor VMM's RX queue, notify Tor VMM
```

The Linux guest **cannot bypass the Firewall** because there is no
direct channel from the NIC driver to the Linux VMM. The SDF
doesn't permit it, and the Microkit promise says no interactions are
possible beyond what's in the SDF. [[3]](https://sel4.systems/About/seL4-whitepaper.pdf)

#### 10.3.3 The kill switch

A "kill switch" (per the task description) is the ability to
instantly stop all network traffic. With this architecture, it's
trivial: the Firewall PD receives a notification on a control
channel (e.g. from a hardware button PD, or from a privileged
control PD), and from that point forward it drops *every* packet.
The NIC driver keeps receiving packets, but they all go to the
drop path. No code change required elsewhere in the system.

(Tails achieves something similar with `iptables -F` + network
namespace teardown — but a Linux kernel exploit can disable that.
A WLTIOS kill switch in the Firewall PD cannot be disabled by
Linux, because Linux has no capability to the Firewall PD.)

#### 10.3.4 Effort estimate

**Low.** The LionsOS firewall is ~5-10 KLOC of C across all its PDs
and runs on QEMU AArch64 + the Compulab IOT-GATE-IMX8PLUS today.
[[1]](https://lionsos.org/docs/examples/firewall/) The work for
WLTIOS:
- Port to x86_64_generic_vtx (LionsOS is ARM-only currently — but
  the sDDF net stack is arch-independent).
- Adapt the rule format to WLTIOS's needs (e.g. "only allow Tor
  traffic from the Tor VM, only allow DNS to a specific resolver,
  drop everything else by default").
- Add the kill-switch control channel.
- Wire it into the WLTIOS SDF.

Estimated effort: 2-4 weeks for a working prototype on QEMU.

### 10.4 Boot Verifier Partition

**Design pattern: TPM-backed measured-boot + runtime attestation PD.**

We have a thorough reference: Nicholas Berridge-Argent's 2021 UNSW
thesis, *"Secure Boot and TPM Attestation for seL4-Based Systems"*. [[2]](https://nickba.dev/thesis/thesis.pdf)

#### 10.4.1 What it does

The Boot Verifier partition:
1. **At boot**: measures (hashes) the seL4 kernel image, the capDL
   loader, every PD's ELF, and the SDF itself. Stores measurements
   in TPM Platform Configuration Registers (PCRs).
2. **At runtime**: periodically (or on-demand) reads the PCRs,
   signs them with a TPM-attestation key, and exposes the signed
   quote to a remote verifier (or to a local UI).
3. **On measurement mismatch**: refuses to release certain secrets.
   E.g. the Key Vault's master key is "sealed" to expected PCR
   values; if the measurements don't match, the TPM won't unseal
   it.

#### 10.4.2 The seL4-specific design

Berridge-Argent's thesis evaluates existing approaches and proposes
a CAmkES-based TPM module for seL4 on ARM (using OP-TEE as a
software TPM, or hardware TPMs via SPI/I2C). Key findings:

- **TPM attestation is possible on ARM seL4 systems** (demonstrated).
- **The clean design is to expose the TPM as a CAmkES/Microkit
  component** that other PDs call via ppcall.
- **x86 systems** (which is WLTIOS's target) typically have a
  hardware TPM (or Intel PTT / AMD fTPM) accessible via TIS
  interface at a fixed MMIO address — *much* simpler than ARM.
- seL4's TCB is small enough that the attestation story is
  meaningful — unlike Linux where the TCB includes millions of
  lines of kernel + drivers.

> "seL4 is a microkernel, with strong security properties guaranteed
> by a unique design, small Trusted Computing Base (TCB), and
> comprehensive formal verification. It is popular in embedded
> systems… However, seL4 has a pre-defined scope and is not designed
> to protect against attacks from outside this scope… Secure boot
> and TPM attestation are two important technologies for validating
> the integrity of a platform." [[2]](https://nickba.dev/thesis/thesis.pdf)

#### 10.4.3 Concrete design for WLTIOS

The Boot Verifier PD:
- **At init**: maps the TPM's MMIO region (TCM TIS at `0xFED40000`
  on x86), opens a session, extends PCRs with measurements of the
  system image.
- **Performs sealed-key retrieval**: asks the TPM to unseal the Key
  Vault's master key (only succeeds if PCRs match expected values).
  Passes the key to the Key Vault PD via a private channel.
- **At runtime**: responds to `ppcall(OP_ATTEST)` from a control PD
  by reading PCRs and producing a signed quote.
- **Periodically**: re-measures critical PDs (optional, advanced
  feature for v2).

#### 10.4.4 Effort estimate

**Low.** The thesis contains working code. The work for WLTIOS:
- Port Berridge-Argent's CAmkES TPM module to Microkit.
- Adapt to x86 hardware TPM (simpler than ARM fTPM).
- Write the measurement-extension code.
- Write the key-unseal flow that hands the master key to the Key
  Vault.

Estimated effort: 2-3 weeks for a working prototype on QEMU (QEMU
has a software-emulated TPM at `-device tpm-tis,tpmdev=tpm0`).

### 10.5 Auth Service Partition

**Design pattern: passive server PD, same shape as Key Vault.**

The Auth Service handles:
- Login (verifying a password/PIN against a stored hash).
- Session token issuance (signed tokens for Linux apps to use).
- Permission checks (does this app have access to this resource?).

It's structurally identical to the Key Vault: a passive server PD
that Linux calls via ppcall. The stored password hash lives in the
PD's private memory. Linux sends `OP_LOGIN(user, password_hash)`
and gets back a signed session token.

For v1 we can probably fold Auth into the Key Vault (one PD doing
both crypto and auth). Splitting them later is trivial (just add a
new PD to the SDF). Recommend: **start with Auth folded into Key
Vault for v1, split in v2**.

---

## 11. Putting It All Together — A WLTIOS v1 Partition Skeleton

Drawing on everything above, here's a concrete v1 target (QEMU
x86_64):

```
┌─────────────────────────────────────────────────────────────────┐
│                       seL4 kernel (verified)                      │
│                      + Microkit initialiser                       │
└─────────────────────────────────────────────────────────────────┘
                                  ▲
                                  │ capabilities (capDL)
                                  │
   ┌──────────────────────────────┴──────────────────────────────┐
   │                       Microkit system                         │
   │                                                              │
   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
   │  │   VMM    │  │   VMM    │  │   NIC    │  │     Boot     │ │
   │  │  (Linux) │  │  (Tor    │  │  driver  │  │   Verifier   │ │
   │  │          │  │   VM)    │  │   PD     │  │      PD      │ │
   │  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
   │       │             │             │               │         │
   │       │             │             ▼               │         │
   │       │             │      ┌──────────────┐       │         │
   │       │             │      │  Firewall PD │       │         │
   │       │             │      │  (rules +    │       │         │
   │       │             │      │   kill sw)   │       │         │
   │       │             │      └──────┬───────┘       │         │
   │       │             │             │               │         │
   │       │             │             │               ▼         │
   │       │             │             │      ┌──────────────┐   │
   │       └─────────────┴─────────────┴─────▶│  Key Vault   │   │
   │              ppcall (ch 2)               │  + Auth PD   │   │
   │                                          │  (passive)   │   │
   │                                          └──────────────┘   │
   │                                                              │
   │   Channels (defined in SDF, enforced by seL4):               │
   │     VMM(Linux) ↔ Firewall      (virtio-net, sDDF queue)      │
   │     VMM(Tor)   ↔ Firewall      (virtio-net, sDDF queue)      │
   │     VMM(Linux) ↔ Key Vault     (virtio-console, ppcall)      │
   │     VMM(Tor)   ↔ Key Vault     (virtio-console, ppcall)      │
   │     NIC drv    ↔ Firewall      (sDDF net queue)              │
   │     Boot Ver   ↔ Key Vault     (one-shot key handoff)        │
   │                                                              │
   │   NO channel between:                                        │
   │     VMM(Linux) ↔ NIC driver   (Linux can't bypass firewall) │
   │     VMM(Linux) ↔ VMM(Tor)     (Linux can't talk to Tor       │
   │                                  except through firewall)    │
   │     Tor VM     ↔ Key Vault frames  (keys are private)        │
   └──────────────────────────────────────────────────────────────┘
```

Memory budget (rough):
- seL4 kernel: ~16 KLOC, ~150 KB binary
- Microkit initialiser + monitor: ~500 KB
- VMM (Linux): ~2 MB binary, 256-512 MB guest RAM
- VMM (Tor VM): ~2 MB binary, 64-128 MB guest RAM
- Firewall PD: ~1-2 MB binary, 8 MB shared queues
- Key Vault + Auth PD: ~500 KB binary, 4 MB shared buffers
- Boot Verifier PD: ~500 KB binary
- NIC driver PD: ~500 KB binary

Idle total: well under 1 GB. Within the 256 MB idle target is
achievable *if* the Linux guest is aggressively slimmed (buildroot
or custom musl-based distro, no GNOME, no systemd).

---

## 12. Open Questions for the WLTIOS Team

1. **Tor partition strategy (§10.1):** Confirm we go with Option B
   (Tor in a second VM) for v1 and write ADR-03 to record the
   decision. The alternative (Option A — port Tor to Microkit) is
   6-12 months of work that doesn't fit the 1-2 year timeline.

2. **CAmkES or Microkit for the VMM (§3.1):** Decide based on a
   spike — can `libvmm`'s `simple` example boot on QEMU x86_64
   within 1 day? If yes, go Microkit everywhere. If no, fall back
   to CAmkES VMM for Phase 1.

3. **Crypto library for Key Vault (§10.2.3):** monocypher or
   libsodium? Monocypher is easier to audit; libsodium has more
   primitives. Recommend monocypher for v1.

4. **Auth as separate PD or folded into Key Vault (§10.5):** Start
   folded; split later. Confirm.

5. **Timing-channel acceptance:** Are we OK with the fact that
   seL4's proofs don't cover timing channels for v1? If not, we'd
   need to wait for cache-colouring support to mature, which is
   research-grade. Recommend: accept for v1, document as a known
   residual risk in ADR-04.

6. **TPM dependency:** Do we require a TPM (hardware or fTPM) on
   the target hardware? If yes, Boot Verifier works as designed
   (§10.4). If no, we fall back to a hardcoded master key for v1
   (insecure against physical access).

7. **Multi-core:** The Microkit manual mentions SMP configs
   (`smp-debug`, `smp-release`). [[5]](https://github.com/seL4/microkit/blob/main/docs/manual.md)
   Do we want SMP from day one (more complex, faster) or start
   uni-core (simpler, slower)? Recommend uni-core for v1.

---

## 13. Recommended Next Actions (in priority order)

### Immediate (before any WLTIOS partition code)

1. **Write ADR-03** recording: "WLTIOS uses Microkit + libvmm as
   the seL4 framework. Critical services are Microkit PDs. The Tor
   Gateway runs in a second Linux VM (Option B) for v1." Cite this
   document.
2. **Spike: boot libvmm `simple` example on QEMU x86_64.** This
   is the gating experiment for the framework decision. 1-2 days
   of work.
3. **Spike: boot the LionsOS firewall on QEMU AArch64** (it works
   there today). This proves the Firewall partition pattern. 1 day.
4. **Spike: boot the seL4 + camkes-vm-examples `minimal_64` on
   QEMU x86_64.** Already planned in the companion research doc.
   Confirms CAmkES VMM still works as a fallback.

### Short-term (Week 1-4 of Phase 1)

5. **Define the v1 SDF** (the skeleton in §11 is the starting
   point). Get it reviewed by the seL4 community (mailing list or
   Discourse) before writing any PD code.
6. **Write the Key Vault PD skeleton.** Empty `init`/`notified`/
   `protected` entry points; link against monocypher; stub the
   ppcall handlers. ~500 LOC.
7. **Write the Firewall PD skeleton.** Adapt the LionsOS firewall
   PD's main loop; empty rule table. ~500 LOC.
8. **Set up the Nix-based build.** Pin the Microkit SDK and seL4
   kernel versions. Reproducible builds from day one.

### Medium-term (Month 2-4)

9. **Get a working end-to-end demo:** Linux guest boots, sends a
   ping packet through the Firewall PD, the Firewall logs it and
   forwards to the (stub) Tor VM. No real Tor yet — just packet
   plumbing.
10. **Add the Key Vault ppcall path.** Linux app encrypts a file
    via `/dev/keyvault`; Key Vault PD returns ciphertext. Verify
    with fault injection that Linux can't read the key.
11. **Add the Boot Verifier PD.** Read PCRs at boot, seal a dummy
    secret, unseal it on demand. Real key sealing in Month 4-5.

### Long-term (Month 5+)

12. **Real Tor integration.** Stand up the Tor VM, get circuits
    working, route Firefox-in-Linux through SOCKS5 → Tor VM.
13. **Kill switch hardware button.** Wire a GPIO (or ACPI power
    button event) to a control PD that notifies the Firewall PD
    to drop all traffic.
14. **Performance tuning.** Hit the boot < 3s, idle < 256 MB
    targets from the PRD.
15. **Begin Option A research** (porting Tor to Microkit natively)
    as a parallel research track for v2.

---

## Sources

| # | Source | URL |
|---|---|---|
| 1 | LionsOS Firewall example docs | https://lionsos.org/docs/examples/firewall/ |
| 2 | Berridge-Argent: "Secure Boot and TPM Attestation for seL4-Based Systems" (UNSW thesis 2021) | https://nickba.dev/thesis/thesis.pdf |
| 3 | seL4 whitepaper (PDF) | https://sel4.systems/About/seL4-whitepaper.pdf |
| 4 | seL4 Summit 2024: "Transitioning from CAmkES VMM to MicroKit VMM" (PDF) | https://sel4.systems/Summit/2024/slides/camkes-to-microkit.pdf |
| 5 | Microkit User Manual v2.2.0 (raw markdown) | https://github.com/seL4/microkit/blob/main/docs/manual.md |
| 6 | CAmkES Manual | https://docs.sel4.systems/projects/camkes/manual.html |
| 7 | The seL4 Microkit project page (Trustworthy Systems) | https://trustworthy.systems/projects/microkit |
| 8 | LionsOS documentation | https://lionsos.org |
| 9 | seL4 Device Driver Framework (sDDF) GitHub | https://github.com/sel4-cap/sDDF |
| 10 | seL4/camkes-vm-examples GitHub | https://github.com/seL4/camkes-vm-examples |
| 11 | CAmkES to seL4CP Transition Guide (PDF) | https://trustworthy.systems/projects/microkit/2304-11-camkes-to-sel4cp.pdf |
| 12 | libvmm — A VMM for seL4 (GitHub + manual) | https://github.com/au-ts/libvmm |
| 13 | capDL: seL4 docs | https://docs.sel4.systems/projects/capdl |
| 14 | capDL Language Specification | https://docs.sel4.systems/projects/capdl/lang-spec.html |
| 15 | Heiser: "How to (and how not to) use seL4 IPC" (microkerneldude blog, 2019) | https://microkerneldude.org/2019/03/07/how-to-and-how-not-to-use-sel4-ipc/ |
| 16 | Microkit Tutorial Part 2 — Client | https://docs.sel4.systems/projects/microkit/tutorial/part2.html |
| 17 | Microkit Tutorial Part 3 — Wordle server | https://docs.sel4.systems/projects/microkit/tutorial/part3.html |
| 18 | CAmkES Cross-VM Connectors tutorial | https://docs.sel4.systems/Tutorials/camkes-vm-crossvm.html |
| 19 | Colias Group: Rust seL4 Microkit bindings | https://coliasgroup.com/seL4-summit-2024-rust-training/rustdoc/microkit/aarch64-sel4-microkit/doc/sel4_microkit/index.html |
| 20 | Microkit Tutorial Part 4 — Virtual machines | https://docs.sel4.systems/projects/microkit/tutorial/part4.html |
| 21 | seL4 Proofs page | https://sel4.systems/Verification/proofs.html |
| 22 | seL4 Foundation FAQ | https://sel4.systems/About/FAQ.html |
| 23 | Heiser: "10 Years seL4: Still the Best, Still Getting Better" (microkerneldude blog, 2019) | https://microkerneldude.org/2019/08/06/10-years-sel4-still-the-best-still-getting-better/ |
| 24 | seL4 Discourse: SIG VMM — current efforts | https://sel4.discourse.group/t/sel4-virtualization-support-current-efforts/665 |
| 25 | seL4 Microkit tutorial Part 1 — Serial server | https://docs.sel4.systems/projects/microkit/tutorial/part1.html |
| 26 | UNSW CS9242 lecture: "Using seL4" (PDF) | https://cgi.cse.unsw.edu.au/~cs9242/23/lectures/01b-sel4.pdf |
| 27 | seL4 docs: VMM library (libsel4vmmplatsupport) | https://docs.sel4.systems/projects/virtualization/docs/libsel4vmm.html |
| 28 | seL4 Summit 2022: sDDF talk (PDF) | https://sel4.systems/Foundation/Summit/2022/slides/d1_06_The_seL4_Device_Driver_Framework_(sDDF)_Lucy_Parker.pdf |
| 29 | seL4 Summit 2023: sDDF talk (PDF) | https://sel4.systems/Summit/2023/slides/day1-08-parker.pdf |
| 30 | YouTube: Transitioning from CAmkES VMM to MicroKit VMM | https://www.youtube.com/watch?v=5xE24p-RauQ |
| 31 | YouTube: The seL4 Microkit — Ivan Velickovic (UNSW) | https://www.youtube.com/watch?v=1Y6uE3eZWKQ |
| 32 | LionsOS: arXiv paper (Jan 2025) | https://arxiv.org/html/2501.06234v1 |
| 33 | seL4 docs: CAmkES VMM | https://docs.sel4.systems/projects/camkes-vm |
| 34 | seL4 docs: Hello CAmkES tutorial | https://docs.sel4.systems/Tutorials/hello-camkes-0.html |
| 35 | seL4 docs: Libraries (IPC) tutorial | https://docs.sel4.systems/Tutorials/libraries-2.html |
| 36 | DornerWorks: Intro to seL4 microkernel | https://www.dornerworks.com/blog/intro-to-sel4-microkernel |
| 37 | seL4 Foundation: How to use seL4 | https://sel4.systems/About/how-to-use.html |

---

## Appendix A: Glossary

- **PD (Protection Domain)** — Microkit's process abstraction: isolated
  address space + thread + capability space + scheduling parameters.
- **SDF (System Description File)** — the XML file that declares all PDs,
  memory regions, channels, and VMs in a Microkit system.
- **Channel** — a unidirectional or bidirectional connection between
  exactly two PDs that carries notifications (async) and/or protected
  procedure calls (sync).
- **Notification** — async, non-blocking, binary semaphore signal sent
  over a channel. Carries no data.
- **PPC (Protected Procedure Call)** — sync, blocking, RPC-like call
  over a channel. Up to 64 machine words of arguments.
- **Memory Region (MR)** — a range of physical memory that can be mapped
  into one or more PDs at specific virtual addresses with specific
  permissions.
- **capDL** — Capability Description Language. The lower-level
  representation of a seL4 system (all kernel objects + all
  capabilities). Microkit generates this from the SDF; the system
  designer normally doesn't write it by hand.
- **CSpace** — a PD's capability space: the data structure holding all
  capabilities (access rights) that PD has.
- **VSpace** — a PD's virtual address space.
- **TCB** — Thread Control Block. The seL4 kernel object representing a
  thread of execution.
- **Passive PD** — a PD whose scheduling context is revoked after init;
  it runs only when called via ppcall or notified. CPU usage charged to
  the caller.
- **vCPU** — virtual CPU object. A PD that manages a VM has one or more
  vCPUs.
- **VMM** — Virtual Machine Monitor. A PD that manages a VM. In
  Microkit/libvmm, this is a regular PD that uses libvmm library code.
- **sDDF** — seL4 Device Driver Framework. Provides standard interfaces
  and protocols for device drivers (NIC, block, serial, I²C, audio) as
  seL4 userspace programs. Uses lock-free queues and model-checked
  signalling.
- **virtio** — paravirtualized device standard. Linux loads virtio
  drivers; the VMM (or a backend PD) emulates the device. libvmm
  implements virtio-net, -block, -console, -sound using sDDF backends.
- **Passthrough** — giving a guest VM direct (unmediated) access to a
  physical device via IOMMU isolation. The VM bypasses VMM and seL4 for
  MMIO; interrupts still route through.
- **VT-x** — Intel virtualization extensions (CPU). seL4 uses these to
  run as a type-1 hypervisor on x86.
- **VT-d** — Intel IOMMU. Constrains DMA from PCI devices to authorised
  frames.
- **TPM** — Trusted Platform Module. Hardware root of trust for
  measured boot and attestation.
- **PCR** — Platform Configuration Register. TPM register that
  accumulates measurements (hashes) of boot stages.
- **MCS kernel** — Mixed-Criticality System kernel config of seL4.
  Introduces scheduling contexts and passive servers. Used by Microkit;
  verification in progress.
- **LionsOS** — UNSW's seL4-based general-purpose OS, built on Microkit
  + sDDF. The closest existing analog to WLTIOS's architecture.
- **libvmm** — UNSW/au-ts library for building VMMs on Microkit.
  Successor to the CAmkES VMM libraries.

---

## Appendix B: One-Paragraph Executive Summary for Kushal

All five planned WLTIOS partitions (Tor Gateway, Key Vault, Firewall,
Boot Verifier, Auth Service) are buildable on seL4 today. The **Firewall
is the easiest** — UNSW's LionsOS project has a working Microkit-based
firewall example we can adapt almost directly. The **Boot Verifier is
also easy** — a 2021 UNSW thesis demonstrated TPM attestation on seL4
with working code. The **Key Vault and Auth Service are medium effort**
— they're passive server PDs that hold secrets in private memory and
expose crypto operations via protected procedure calls; Linux can never
read the keys because seL4's formal proofs guarantee it. The **Tor
Gateway is the hard one** — Tor is a 400 KLOC POSIX program that
expects a libc, sockets, and a filesystem, none of which a Microkit PD
provides natively. The pragmatic v1 approach is to run Tor in a *second*
Linux guest VM (isolated from the main Linux guest by seL4), with
Tor's identity keys held in the Key Vault PD so even a Tor VM
compromise can't leak them. The recommended framework is **Microkit +
libvmm** (the seL4 Foundation's official recommendation), with the
caveat that the libvmm x86-64 VM support is still experimental and
may force a temporary fallback to the older CAmkES VMM. Next concrete
action: write ADR-03 recording these decisions, then run a 1-2 day
spike attempting to boot libvmm's `simple` example on QEMU x86-64 to
validate the framework choice.
