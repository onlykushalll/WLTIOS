# WLTIOS — Architecture (Consolidated Reference)

> **Supersedes:** the original `docs/ARCHITECTURE.md` (2026-06-20 blueprint),
> `docs/MODULE_MAP.md`, `docs/FILETREE.md`, and the architecture sections of
> `docs/context/MASTER_CONTEXT.md` / `PROJECT_CONTEXT_SUMMARY.md`. Those are
> archived under `_ARCHIVED_2026-07-29` suffixes in their original folders.
> This document reflects the **actual current architecture**, verified
> against source and `git log`/`git show` on 2026-07-29, not the original
> aspirational blueprint.
> **Status:** Living document — re-verify against the repo before trusting
> any status line here past its date.

---

## 1. What WLTIOS actually is today

WLTIOS is not one unified seL4-based system yet. Per **ADR-10** (2026-07-19,
evidence gathered directly from `CMakeCache.txt` / `app_settings.cmake` /
`file` on kernel images — not inference), the project currently has **three
parallel, unreconciled build tracks** sharing one seL4 source checkout:

| Track | What it is | Arch | Status |
|---|---|---|---|
| **Track 1 — seL4 multi-PD (32-bit)** | KeyVaultPD + TorPD as CAmkES components/VMs | `ia32` | KeyVaultPD ping/pong verified 2026-07-12. TorPD two-VM boot fixed 2026-07-25 (commit `9897506`, heap_size bug) — build reaches Buildroot login. The vm0-vm1 IPC round-trip proof from ADR-09 was not independently re-confirmed in this consolidation pass — re-verify serial log before relying on it. |
| **Track 2 — Desktop/compositor** | Standalone 64-bit Linux kernel, **no seL4 in the boot path** | `x86_64` | This is where nearly all GUI/AI/app work happens (compositor, shell, Kairos, wlt-pkg, etc.) |
| **Track 3 — 64-bit seL4** | `build-minimal-64`, bare VM only | `x86_64` | Proves 64-bit seL4 VMs work in this repo; not yet carrying KeyVaultPD/TorPD |

**Practical implication:** when this document or others say "the compositor
runs," that is Track 2 (Linux kernel booted directly in QEMU) — the seL4
security architecture (Tor isolation, Key Vault, Firewall PD) is **not**
protecting the desktop session today. Track 1 is a real, separately-verified
proof that seL4 multi-PD isolation works technically; unifying Track 1 and
Track 2 so the desktop actually runs under seL4 is future work (ADR-10 Step 3,
unscheduled as of this writing).

---

## 2. Layered design (as originally specified, status per layer below)

```
USER APPLICATIONS  (terminal, file manager, settings, editor, task mgr, ...)
        |
WLTIOS USERSPACE (custom C, tens of thousands of real lines per the
                   orphaned-code + A/B audits, not the ~3M line estimate
                   in the original blueprint)
        |
LINUX KERNEL  (reused; runs as a guest VM under seL4 in Track 1, runs
               directly under QEMU with no seL4 in Track 2 - see above)
        |
seL4 MICROKERNEL  (verified; Track 1/3 only - not in Track 2's boot path)
        |
BOOTLOADER (WLT-Boot, custom UEFI loader, real Ed25519 verified boot
            as of commit c169cb2, 2026-07-28)
```

The original blueprint's line-count table (~3M custom / ~30M reused) was a
target estimate, not a measurement, and should not be quoted as current fact.
The **Orphaned Code Audit** (2026-07-16, corrected 2026-07-18) is the most
reliable real measurement available for the modules it covered: `wlt-ai`
5,114 lines (19 files), `wlt-permissions` 1,246 lines (clean, zero stubs),
`wlt-tor-mgr` 2,060 lines (real Tor control-port client), `wlt-pkg` 827 lines
at time of audit (heavily stubbed then; substantially reworked since — see
the package-management doc), `wlt-update` 1,525 lines (later re-audited at
2,088 lines including headers, found NOT a skeleton).

---

## 3. seL4 layer (Track 1 / 3)

### Protection Domains (PDs) - status

| PD | Purpose | Status |
|---|---|---|
| **Key Vault PD** | Holds encryption keys, does crypto ops, keys never leave the partition | Ping/pong across the seL4 kernel boundary verified real, 2026-07-12 (raw serial log: `[key-vault-pd] ping received, pong sent`, IRQ counter incremented). An FD-based key-delivery bridge client (`abe3d84`, 2026-07-23) exists for `wlt-update`'s dm-verity work, but the bridge daemon reading from `/dev/virtio-port/wltkv` still needs seL4 KeyVaultPD + virtio-console wiring (documented as open work in the update spec). |
| **TorPD** | Isolate the Tor daemon in a second Linux guest VM (ADR-03, ADR-09) | Scaffold + two-VM boot fixed 2026-07-25 (`9897506`) after a memory-collision bug (heap_size 500x too large, root-caused via the `optiplex9020.camkes` reference example). Build reaches Buildroot login. The SOCKS-proxy relay (Phase 2 of the TorPD spec) and the vm0-vm1 IPC proof are not confirmed real in this pass - re-verify before treating TorPD as functionally isolating Tor traffic. |
| **Boot Verifier PD** | Runtime integrity re-check of kernel/config hashes | Scaffold added 2026-07-26 (`eddb02d`, modeled on KeyVaultPD's bare-component pattern), with a real SHA-256 comparison (not a stub-OK) landed 2026-07-27 (`16ef5b9`). Not yet wired to trigger rollback/kill-switch on mismatch - treat as a working comparison primitive, not the full spec'd runtime-tamper-detection loop. |
| **Firewall PD** | Enforce Tor-only routing + kill switch at the seL4 level | Not started. The kill switch that exists today (see networking doc) is a route-deletion fallback implemented in Linux userspace, not seL4 enforcement. |

### Why the 32-bit/64-bit split exists

seL4's 32-bit (`ia32`) VMM uses software LAPIC emulation, which WSL2's nested
KVM supports; the 64-bit VMM needs hardware APIC virtualization
(`vmx-apicv-xapic`), which WSL2 doesn't expose. This is why development
happened on 32-bit even though a 64-bit seL4 build exists and boots
(`build-minimal-64`, a plain VM with no PDs). Full detail in
`docs/adr/10-architecture-fork.md` (kept, not archived - still accurate).

### seL4 -> NIC passthrough (Track 1)

Never achieved. Multiple genuinely distinct attempts (untyped-pool sizing,
IOSpace cap allocation) all failed at `capdl-loader`'s `create_objects`
running out of untyped memory - root-caused as a kernel-bootinfo-untyped-cap
ceiling (`KernelMaxNumBootinfoUntypedCaps=230`), not fixable by config alone.
See `docs/context/SEL4_PASSTHROUGH_DEFINITIVE_ARCHIVED_2026-07-29.md` (kept
for reference, content folded in above) for the full attempt log. Track 2's
desktop networking bypasses this entirely by not using seL4 at all.

---

## 4. Linux guest / Track 2 desktop stack

- **Kernel:** custom-built Linux 5.10.218 (per `packages/kernel/wlt-kernel.config`),
  with `CONFIG_E1000=y` and `CONFIG_VIRTIO_NET=y` built in (not modules) -
  confirmed via commit `ab054d6`. Older docs citing "Linux 4.8.16"
  (the archived SECURITY.md, security-whitepaper.md, MODULE_MAP.md)
  describe an earlier Buildroot-based boot from June 2026 and are stale.
- **Networking:** real eth0 bring-up via raw ioctl (`qemu_bringup.c`), Tor
  reported bootstrapping to 100% (see networking doc for what's independently
  re-verified vs. only build-reported).
- **Compositor:** wlroots 0.18.2-based, currently **pixman/software
  rendering** (`WLR_RENDERER=pixman`) because QEMU's virtio-gpu doesn't
  support EGL/GL contexts in this environment. A conditional GLES2 renderer
  path (`c30197a`, 2026-07-22) and a real dual-pass Gaussian backdrop blur
  shader (`b2d9b08`, 2026-07-23) were added as the first steps of the
  GPU/glassmorphism track (see `docs/specs/wlt-design-system_ARCHIVED_2026-07-29.md`,
  content folded into the UI doc - it's a genuine forward-looking technical
  spec, not a stale status claim). Whether GLES2 is actually selected at
  runtime on this QEMU setup vs. falling back to pixman was not re-confirmed
  in this pass.
- **Window management:** real - multi-window desktop (terminal, file
  manager, launcher, statusbar, settings, Kairos) renders simultaneously,
  with resize/minimize/maximize/drag/Alt+Tab, verified via QEMU screendump +
  VLM pixel description (2026-07-12) and confirmed still building as of
  `9c30d8e` (2026-07-23 Makefile/`-Werror` fix).

---

## 5. Technology stack (as actually used, not the original per-component table)

| Component | Language | Notes |
|---|---|---|
| Bootloader (WLT-Boot) | C + inline asm | UEFI, real Ed25519 verified boot (monocypher-backed) |
| seL4 PDs | C (bare-metal, no libc) | Key Vault, TorPD (Linux guest), Boot Verifier |
| Compositor, shell, terminal, file manager, settings, editor, task manager, screenshot tool | C | Built on wlroots; all real, not skeletons, per orphaned-code audits |
| wlt-pkg (package manager) | C | Original spec said Rust; implementation is C (undocumented-decision, flagged in status/roadmap doc) |
| wlt-update (A/B updater) | C | Spec says Rust (`Cargo.toml`); implementation is C, substantially complete (2,088 lines, zero stubs, needs libsodium-dev at build time) |
| wlt-net, wlt-sandbox | C | Specs say Rust; implementation is C (flagged in the original `MODULE_MAP.md`'s own "Known Issues" list, and still true) |
| wlt-ai / Kairos | C | Not Rust+Python as the original spec's file-structure section describes |
| wlt-defender (Guardian) | C + eBPF (C, `-target bpf`) | See AI/Guardian doc |

---

## 6. Filesystem / package layout (target design, largely not yet realized)

The original blueprint's `/system`, `/users`, `/persist` squashfs+LUKS
layout, and the Nix-style `/var/wlt/store/` symlink-tree package layout, are
both still just **design targets** - `wlt-pkg`'s real install/remove
lifecycle (verified 2026-07-19) operates against a plain local test package,
not the full store/symlink/rollback design described in the package spec.
See the package-management doc for current truth.

---

## 7. Where to look for more detail

- **Security architecture, threat model, verified boot, LUKS:** `docs/SECURITY.md`
- **AI (Kairos) + Guardian security engine:** `docs/AI_KAIROS.md`
- **Boot sequence + A/B update/rollback:** `docs/BOOT_AND_UPDATES.md`
- **Networking, Tor, kill switch, TorPD detail:** `docs/NETWORKING_TOR.md`
- **Package manager:** `docs/PACKAGE_MANAGEMENT.md`
- **Compositor, shell, apps, design system:** `docs/UI_COMPOSITOR.md`
- **What's actually done vs. planned, dated:** `docs/STATUS_ROADMAP.md`
- **Build rules, agent workflow, coding style:** `docs/PROCESS_METHODOLOGY.md`
- **Terms, module list, build/user-guide essentials:** `docs/GLOSSARY_REFERENCE.md`
