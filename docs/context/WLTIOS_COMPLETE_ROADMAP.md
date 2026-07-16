# WLTIOS — The Complete Roadmap (Original Idea → 1.0 → Beyond)

> **Grounded in:** `docs/PRD.md`, `docs/ROADMAP.md`, `docs/ARCHITECTURE.md`,
> and all 23 specs in `docs/specs/`. Not hallucinated — every feature below
> is sourced from the actual project documents.
> **Last updated:** 2026-07-11

---

## 1. The Original Idea (Your Pitch, Verbatim from the PRD)

**WLTIOS** = **W**indows + **L**inux + **T**ails + **I**nspired **OS**

From the PRD §1:
> WLTIOS is a general-purpose desktop operating system that combines the
> usability of Windows, the power and hardware support of Linux, the
> anonymity of Tails, and the verified security of seL4 — with AI natively
> integrated at the OS level.

**The five pillars (from ARCHITECTURE.md §1):**
1. **Windows-inspired UX** — familiar, flexible, user-friendly interface
2. **Linux-powered foundation** — proven kernel, unmatched hardware support, Unix philosophy
3. **Tails-grade anonymity** — Tor everywhere, amnesic by default, MAC spoofing, kill switch
4. **seL4 verified isolation** — mathematical security guarantees for critical services
5. **AI-native integration** — built-in AI assistant at the OS level (not an app)

**The problem you're solving (PRD §1):**
> No existing OS combines all of these. Users currently must choose between
> usability, security, anonymity, and AI. WLTIOS eliminates the compromise.

**Target users (PRD §1):**
- Primary: Privacy-conscious power users who want a daily-driver OS
- Secondary: Developers who need a flexible, scriptable, secure environment
- Tertiary: Journalists, activists, researchers who need anonymity + usability

**The 10 design principles (ARCHITECTURE.md §2):**
1. Everything is a file (Unix philosophy)
2. Secure by default (Tor on, telemetry off, disk encrypted, no root login)
3. Minimal footprint (<256MB RAM idle, <3s boot, <2GB disk)
4. Formal isolation (critical services on seL4, not Linux)
5. AI-native (AI is a first-class OS citizen, not a bolted-on app)
6. No telemetry, ever (zero data leaves unless user explicitly sends)
7. Amnesic option (can run entirely in RAM, no trace on hardware)
8. Familiar but better (taskbar, windows, settings — but improved)
9. Atomic everything (Nix-style updates, installs, rollbacks)
10. Swappable identity (multiple personas/identities for different activities)

**The 7 product goals (PRD §2):**
| # | Goal | Success Metric |
|---|---|---|
| G1 | Bootable, daily-driver OS | Can replace Windows/Linux for typical use |
| G2 | Anonymous by default | All traffic through Tor, no leaks, amnesic option |
| G3 | Fast and lightweight | <256MB RAM idle, <3s boot, <2GB disk |
| G4 | Secure by design | seL4 isolation, verified boot, app sandboxing |
| G5 | AI-native | Built-in AI that sees, hears, helps |
| G6 | Familiar UX | Windows/Linux users feel at home in minutes |
| G7 | Open source | Full source, community contributions |

**Non-goals (PRD §3 — what we're NOT building):**
- ❌ A server OS (desktop/laptop focus)
- ❌ A mobile OS (desktop first; mobile is future)
- ❌ Windows app compatibility / .exe / Wine
- ❌ A Linux distro (full OS, not repackaging)
- ❌ 100% from scratch (reuse Linux kernel + seL4)
- ❌ Backwards compatibility (clean slate, modern hardware only, UEFI x86-64)

---

## 2. The 24-Month Roadmap (from `docs/ROADMAP.md`)

**Timeline:** 24 months (2 years). Start: 2026-06-20. Target 1.0: 2028-06-20.
**Total estimate:** ~4,800 dev hours, ~3M lines of code.

```
Phase 1: Foundation     Months 1-6    →  MVP (boots in QEMU)
Phase 2: Desktop        Months 7-12   →  Alpha (usable desktop)
Phase 3: Security       Months 13-18  →  Beta (full anonymity)
Phase 4: Polish + AI    Months 19-24  →  1.0 (production ready)
```

---

## Phase 1: Foundation (Months 1-6) — MVP

**Goal:** Prove the architecture works. Bootable OS in QEMU.

### Month 1: Setup + Bootloader
- **Week 1-2:** Dev environment (QEMU + OVMF, cross-compiler, Rust, Git, build system). Test: "Hello World" bootloader boots in QEMU.
- **Week 3-4:** WLT-Boot (custom UEFI bootloader in Assembly + C, multiboot2 protocol, load seL4, transfer control).
- **Deliverable:** `wlt-boot.bin` — boots seL4 in QEMU.

### Month 2: seL4 + Linux Integration
- **Week 5-6:** seL4 setup (build seL4 for x86-64, create partition for Linux VM, configure Linux as guest).
- **Week 7-8:** Linux guest config (minimal kernel, initramfs, Linux↔seL4 comm channel).
- **Deliverable:** `wltios.iso` — seL4 + Linux boots in QEMU.

### Month 3: Init System + Shell
- **Week 9-10:** WLT-Init (custom init in C, replaces systemd, service startup, dependency resolution).
- **Week 11-12:** Minimal shell (command interpreter, core commands: ls/cd/cat/echo/mkdir/rm, path resolution).
- **Deliverable:** Bootable OS with working shell.

### Month 4: Terminal + File Manager
- **Week 13-14:** Terminal emulator (C + framebuffer, VT100 escape sequences, PTY to shell).
- **Week 15-16:** Basic file manager (list/navigate/copy/move/delete).
- **Deliverable:** OS with terminal + file manager.

### Month 5: Basic GUI
- **Week 17-18:** Framebuffer + input (display, keyboard, mouse).
- **Week 19-20:** Basic window system (move/resize/close windows, render text, multiple windows).
- **Deliverable:** OS with basic GUI windows.

### Month 6: Tor + MVP
- **Week 21-22:** Network + Tor (configure Linux network stack, install Tor daemon, route traffic through Tor, access .onion sites).
- **Week 23-24:** MVP polish (fix bugs, optimize boot, document build).
- **MVP Deliverable:** Boots in QEMU <5s, custom bootloader, seL4+Linux, terminal, file manager, basic GUI, Tor, idle RAM <512MB.

### Phase 1 Honest Status (as of 2026-07-11)
| Item | Status |
|---|---|
| seL4 boots in QEMU (32-bit) | ✅ Verified |
| seL4 + Linux guest | ✅ Verified |
| seL4 + Tor together | ✅ Verified |
| WLT-Boot (UEFI bootloader) | ✅ Built |
| Verified boot (Ed25519) | ✅ Code-level complete (monocypher, 12/12 tests, PyNaCl cross-checked) |
| WLT-Init | ✅ Built (service.c, mount.c, ipc.c, tor.c, shutdown.c, ram_wipe.c) |
| WLT-Shell | ✅ Built (taskbar, launcher, notifications, tray — skeleton, never visually confirmed) |
| WLT-Terminal | ✅ Built (ansi, pty, render, scrollback) |
| Basic GUI / window system | ⚠️ Compositor exists but headless-only — no visible window ever achieved |
| Tor connectivity | ✅ Verified |
| **MVP overall** | ⚠️ Most components built, but the GUI has never been seen. The "visible window" gap is the blocker for MVP completion. |

---

## Phase 2: Desktop (Months 7-12) — Alpha

**Goal:** Usable desktop environment. Daily-driver potential.

### Month 7-8: Wayland Compositor
- **Week 25-28:** WLT-Compositor (wlroots, outputs, inputs, render surfaces, Wayland apps display).
- **Week 29-30:** Window management (drag move, drag resize, close, minimize, maximize, z-order stacking).
- **Week 31-32:** Window snapping (half-screen left/right, quarter-screen corners, full-screen, Alt+Tab switcher).

### Month 9: Taskbar + Launcher
- **Week 33-34:** WLT-Shell taskbar (running apps, click-to-focus, system tray: network/battery/clock/Tor).
- **Week 35-36:** App launcher (search + launch, index installed apps, search by name, launch on Enter).

### Month 10: Settings + Package Manager
- **Week 37-38:** WLT-Settings (network/WiFi/Tor config, display resolution/brightness, sound volume, user account/password).
- **Week 39-40:** WLT-Pkg (Nix-style package format, install/remove/list/update).

### Month 11: Web Browser + Disk Encryption
- **Week 41-42:** Tor Browser integration (Firefox fork, max privacy config, system Tor integration).
- **Week 43-44:** Disk encryption (LUKS, encrypt home directory, encrypt persistent storage).

### Month 12: App Sandboxing + Alpha Release
- **Week 45-46:** App sandboxing (cgroups wrapper, namespaces wrapper, app manifest format, permission enforcement).
- **Week 47-48:** Alpha polish + release.
- **Alpha Deliverable:** Custom Wayland compositor, full window management, taskbar + launcher, settings, package manager, Tor Browser, disk encryption, app sandboxing (basic), boots on real hardware, idle RAM <384MB.

### Phase 2 Honest Status
| Item | Status |
|---|---|
| WLT-Compositor (Wayland/wlroots) | ⚠️ Exists, exposes Wayland globals, but headless-only. **DRM backend is the standing task.** |
| Window management (snap, drag, resize) | ❌ Not verified (no visible window to test against) |
| Taskbar + launcher | ⚠️ Skeleton code exists, never run visually |
| WLT-Settings | ⚠️ Spec exists; wlt-screenshot utility built for VLM inspection |
| WLT-Pkg | ⚠️ One signed package installs/runs (verified). No real repo, no Tor Browser package, no A/B updates. |
| Tor Browser | ❌ Not built |
| Disk encryption (LUKS) | ⚠️ wlt-crypto/luks.c exists in sandbox; not verified on real disk |
| App sandboxing | ⚠️ wlt-sandbox exists (manifest, launch, monitor, audit); per-PID net isolation code-level complete (69/69 tests) but never integrated |
| **Alpha overall** | ❌ Not reached. The compositor DRM gap blocks all visual verification. |

---

## Phase 3: Security + Anonymity (Months 13-18) — Beta

**Goal:** Full security and anonymity features. Production-ready.

### Month 13-14: seL4 Partitions
- **Week 49-52:** Tor partition (move Tor from Linux to seL4, IPC between Linux and Tor partition).
- **Week 53-54:** Key Vault partition (move encryption keys to seL4, encrypt/decrypt IPC, keys isolated).

### Month 15: Firewall + Boot Verifier
- **Week 55-56:** Firewall partition (seL4 packet filtering, enforce Tor-only rule, kill switch).
- **Week 57-58:** Boot Verifier partition (runtime kernel signature checks, rootkit detection).

### Month 16: Anonymity Features
- **Week 59-60:** MAC randomization + kill switch (randomize MAC every boot, Tor-dies→network-dies, DNS through Tor).
- **Week 61-62:** Amnesic mode (Live USB boot, tmpfs for all writable paths, nothing touches disk unless saved).

### Month 17: Permissions + Privacy
- **Week 63-64:** App permission system (network: tor-only/direct/none, files: sandbox/home/full, camera/mic: allow/deny/ask).
- **Week 65-66:** Privacy badges + secure delete (🟢🟡🔴 per app, shred files, Tor circuit viewer).

### Month 18: Beta Release
- **Week 67-68:** Beta polish (security audit, pentest, bug fix, perf, docs).
- **Beta Deliverable:** seL4 partitions (Tor, keys, firewall, boot verifier), kill switch <100ms, MAC randomization, amnesic mode, app permissions, privacy badges, secure delete, Tor circuit viewer, idle RAM <256MB, boot <3s.

### Phase 3 Honest Status
| Item | Status |
|---|---|
| Key Vault PD | ✅ Code-level complete (26/26 tests). **Both components boot together in seL4** — `[key-vault-pd] alive` verified. Ping/pong pending (standing task Section B). |
| Tor partition | ⚠️ Tor runs inside seL4 but not yet as a separate partition (ADR-03: Tor in second VM) |
| Firewall partition | ⚠️ Kill switch works (route-based, nftables+iptables). seL4 Firewall PD not yet built. |
| Boot verifier | ⚠️ Ed25519 verify works (code-level). Runtime verifier PD not built. |
| MAC randomization | ✅ Verified |
| Kill switch | ✅ Verified, bidirectionally reversible + audit-logged |
| Amnesic mode (tmpfs) | ✅ Verified |
| RAM wipe on shutdown | ⚠️ Userspace best-effort only (7/7 tests). Needs kexec for kernel-space keys. |
| App permission system | ❌ Not implemented (spec exists: wlt-permissions.md) |
| Privacy badges | ❌ Not implemented |
| Secure delete | ⚠️ wlt-crypto/secure_delete.c exists in sandbox |
| Tor circuit viewer | ❌ Not built |
| **Beta overall** | ❌ Not reached. Key Vault PD is closest to done. Tor/Firewall/Boot-Verifier PDs are the big remaining partition work. |

---

## Phase 4: Polish + AI (Months 19-24) — 1.0

**Goal:** Production-ready, polished, AI-integrated.

### Month 19-20: AI Assistant (Kairos)
- **Week 73-76:** WLT-AI core (Rust, LLM integration, tool use, screen understanding via VLM).
- **Week 77-78:** AI UI (always-available sidebar, voice input STT, voice output TTS, system control — open apps, change settings).
- **Week 79-80:** AI features (file search "find my PDF from last week", privacy analysis "which apps are spying?", anonymity coach "you're leaking DNS", optional cloud models Gemini/Claude).

### Month 21: Update System
- **Week 81-82:** A/B partition updates (download to inactive partition, verify signature, reboot into new, auto-rollback if boot fails).
- **Week 83-84:** Package repository (hosted, GPG signing, delta updates, offline install from cache).

### Month 22: Polish
- **Week 85-86:** UI polish (animations, themes dark/light/custom, consistent icons, multilingual fonts).
- **Week 87-88:** Hardware compatibility (multi-PC testing, multi-monitor, Bluetooth, touchpad gestures).

### Month 23: Documentation + Community
- **Week 89-90:** Documentation (user guide, developer guide, security whitepaper, API docs).
- **Week 91-92:** Community setup (website, forum/Matrix, bug tracker, contribution guidelines).

### Month 24: 1.0 Release
- **Week 93-94:** Final testing (30-day daily-driver test, final security audit, perf benchmarks, release candidate).
- **Week 95-96:** 1.0 release (final polish, announcement, ISO distribution, docs published, community launch).
- **1.0 Deliverable:** AI assistant (local + optional cloud), atomic updates with rollback, polished UI, multi-monitor + Bluetooth, full docs, community platform, idle RAM <256MB, boot <3s, daily-driver capable.

### Phase 4 Honest Status
| Item | Status |
|---|---|
| Kairos AI core | ⚠️ Basic inference + tool-calling verified on 1.5B stand-in model |
| Qwythos-9B upgrade | ❌ Not done |
| AI UI (sidebar, voice) | ❌ Not built |
| AI features (file search, privacy analysis, anonymity coach) | ❌ Not built |
| A/B partition updates | ❌ Not implemented (spec exists: wlt-update.md) |
| Package repository | ❌ Not hosted |
| UI polish (animations, themes) | ❌ Not started (the "custom UI" gap from your original pitch) |
| Hardware compatibility (multi-monitor, Bluetooth) | ❌ Not tested |
| Documentation | ⚠️ Specs + ADRs exist; no user guide yet |
| Community platform | ❌ Not set up |
| **1.0 overall** | ❌ Not reached. AI is at stand-in stage, UI is unstarted, updates don't exist. |

---

## 3. All 23 Specs (the feature-level detail)

Every spec in `docs/specs/` with its status:

| Spec | What it defines | Status |
|---|---|---|
| `wlt-boot.md` | UEFI bootloader, verified boot, kernel loading | ✅ Built (Ed25519 verify code-level complete) |
| `wlt-init.md` | Custom init (replaces systemd), service mgmt, mount, shutdown | ✅ Built |
| `wlt-compositor.md` | Wayland compositor on wlroots, outputs, inputs, rendering | ⚠️ Headless-only (DRM backend is standing task) |
| `wlt-compositor-notes.md` | Implementation notes (tinywl reference, wlroots APIs) | ✅ Reference doc |
| `wlt-compositor-fb-export.md` | Headless framebuffer export for VLM screenshots | ✅ Spec + wlt-screenshot utility built |
| `wlt-shell.md` | Taskbar, app launcher, notifications, system tray | ⚠️ Skeleton built, never visually confirmed |
| `wlt-terminal.md` | Terminal emulator, VT100, PTY, scrollback | ✅ Built |
| `wlt-filemanager.md` | File manager GUI | ⚠️ Spec only |
| `wlt-settings.md` | Settings app (network, display, sound, security, privacy, accounts, updates, apps, system, about) | ⚠️ Spec only |
| `wlt-net.md` | Network stack, Tor proxy, kill switch, MAC randomization | ✅ Built + verified |
| `wlt-tor-mgr.md` | Tor daemon management, circuits, new identity | ✅ Built |
| `wlt-crypto.md` | LUKS, file encryption, key vault client, secure delete | ✅ Built (sandbox) |
| `wlt-sandbox.md` | App sandboxing (cgroups, namespaces, manifest, audit) | ⚠️ Built, not verified in production |
| `wlt-permissions.md` | App permission system (network/files/camera/mic) | ❌ Spec only |
| `wlt-pkg.md` | Package manager (Nix-style, install/remove/update) | ⚠️ One signed package works; no repo |
| `wlt-update.md` | A/B partition updates, signature verify, auto-rollback | ❌ Spec only |
| `wlt-ai.md` | Kairos AI assistant (LLM, tool use, VLM, voice, UI) | ⚠️ Stand-in model only |
| `key-vault-pd.md` | seL4 Key Vault partition (keys never leave PD) | ✅ Code-level complete, both components boot |
| `firewall-pd.md` | seL4 Firewall partition (packet filtering, Tor-only) | ❌ Spec only |
| `boot-verifier-pd.md` | seL4 Boot Verifier partition (runtime tamper detection) | ❌ Spec only |
| `wlt-confirm.md` | Confirm-token for irreversible actions | ✅ Code-level complete (12/12 tests) |
| `wlt-pid-net.md` | Per-PID network isolation (cgroup + nftables) | ✅ Code-level complete (69/69 tests) |
| `wlt-ram-wipe.md` | RAM wipe on shutdown (cold-boot defense) | ⚠️ Userspace best-effort only |

---

## 4. The 7 Architectural Decisions (ADRs)

| ADR | Decision | Status |
|---|---|---|
| 01 | UEFI vs BIOS → chose UEFI (Secure Boot compat) | ✅ Accepted |
| 02 | seL4 as base (not Linux-as-host) | ✅ Accepted — the core architectural claim |
| 03 | Tor in a second VM (not the main Linux guest) | ✅ Accepted |
| 04 | Microkit + libvmm (not CAmkES) | ✅ Accepted — de-risk: boot libvmm `simple` on QEMU first. **Current work uses CAmkES (vm-examples) which works; Microkit is the future direction.** |
| 05 | Wayland + wlroots (not X11) | ✅ Accepted |
| 06 | musl libc (not glibc) | ✅ Accepted |
| 07 | Custom init (not systemd) | ✅ Accepted |

---

## 5. The 5 seL4 Protection Domains (the core isolation promise)

| PD | Purpose | Status |
|---|---|---|
| **Linux guest VM** | User-facing OS, hardware support | ✅ Boots |
| **Key Vault PD** | Encryption keys, crypto ops — keys never leave | ✅ Both boot together, ping/pong pending |
| **Tor PD** | Tor daemon isolated from Linux | ⚠️ Tor runs in Linux guest (ADR-03 says second VM, not yet separate PD) |
| **Firewall PD** | Packet filtering, Tor-only enforcement | ❌ Spec only |
| **Boot Verifier PD** | Runtime kernel signature checks | ❌ Spec only |

**This is the architectural claim that makes WLTIOS better than Tails:** Tails runs everything in one Linux kernel — pop that kernel, anonymity is gone. WLTIOS keeps Tor and keys unreachable even if Linux is fully compromised. **That isolation is not real yet** — everything currently runs in one undivided Linux VM under seL4. The Key Vault PD is the first real partition (booting, ping/pong pending).

---

## 6. What's Done vs Not Done (Honest Summary)

### ✅ Genuinely verified (real QEMU boot / serial output / passing tests)
- seL4 boots (32-bit KVM), Linux guest boots, Tor bootstraps
- Kill switch (route-based, bidirectionally reversible, audit-logged)
- MAC randomization
- Amnesia (tmpfs)
- Guardian 5-layer self-test (100/100)
- 23 BPF hooks attached
- YARA detection + enforcement
- FIM tamper detection + auto-restore
- One signed package installs/runs
- Kairos basic inference + tool-calling (1.5B stand-in)

### ✅ Code-level complete (host-sim tests pass, pending real integration)
- Ed25519 verified boot (monocypher, 12/12 + PyNaCl cross-check 8/8)
- Key Vault PD (26/26)
- Key Vault IPC bridge (32/32)
- Confirm-token mechanism (12/12, constant-time)
- Per-PID network isolation (69/69)
- RAM wipe (7/7, userspace best-effort)
- Both seL4 components boot together (`[key-vault-pd] alive`)

### ❌ Not started / spec only
- Compositor DRM backend (the visible-GUI blocker)
- Custom desktop shell (taskbar, launcher, theming — your original pitch feature #1)
- Tor Browser
- App permission system
- Privacy badges
- Tor circuit viewer
- A/B partition updates
- Package repository
- Qwythos-9B model
- AI UI (sidebar, voice, VLM)
- seL4 Firewall PD
- seL4 Boot Verifier PD
- seL4 Tor PD (as separate partition, not just running in Linux)
- kexec wipe-kernel (full cold-boot defense)
- Real hardware boot on the Dell
- Community platform

---

## 7. The Standing Task (unblocks Phase 1 MVP + Phase 3 Beta)

**Section A — Compositor DRM backend** (unblocks all visual verification):
1. Probe GPU device (virtio-gpu-gl → virtio-vga → qxl-vga)
2. Wire DRM-first/headless-fallback in compositor.c
3. Bundle deps (ldd set + Mesa DRI dlopen'd drivers)
4. Boot with VNC, launch wlt-terminal, human verifies "window vs black frame"
5. 90-min cap if stuck

**Section B — seL4 multi-PD ping/pong** (the core isolation proof):
1. Diagnose two "Insufficient memory" messages (8MB, 4MB)
2. Check rootfs for a shell; if none, bundle minimal ping client
3. Read .camkes connection blocks + key_vault_pd.c for wire format
4. Boot + verify `[key-vault-pd] alive` + ping/pong exchange
5. Max 4 distinct attempts, then write definitive analysis

**If B lands:** first real seL4 partition isolation in the project. The thing that makes WLTIOS categorically better than Tails becomes true rather than aspirational.

---

## 8. Beyond 1.0 (future possibilities, not in the 24-month plan)

These aren't in the ROADMAP but have been discussed:
- **Mobile WLTIOS** (PRD non-goal, but future possibility)
- **Windows app compatibility** via Wine (PRD non-goal, separate project)
- **Server edition** (PRD non-goal, different needs)
- **Hardware security key integration** (YubiKey, Nitrokey)
- **Multi-architecture support** (ARM64 laptops, RISC-V future)
- **Federated package mirrors** over Tor
- **Integrated VPN fallback** (if Tor exit nodes blocked)
- **Decentralized identity** (replacing swappable identity with cryptographic personas)

---

## 9. The Resource Reality

| Phase | Duration | Hours | Lines of Code | Status |
|---|---|---|---|---|
| Phase 1 (MVP) | 6 months | ~1,200 | ~150K | ~80% done (GUI gap blocks completion) |
| Phase 2 (Alpha) | 6 months | ~1,200 | ~1M | ~20% done (compositor exists, desktop unstarted) |
| Phase 3 (Beta) | 6 months | ~1,200 | ~800K | ~30% done (Key Vault PD closest, others spec-only) |
| Phase 4 (1.0) | 6 months | ~1,200 | ~1M | ~5% done (Kairos stand-in only) |
| **Total** | **24 months** | **~4,800** | **~3M** | **~30% overall** |

**The honest one-liner:** The backend architecture is proven (seL4+Linux+Tor boots, Guardian works, Key Vault PD boots alongside Linux). The desktop experience — your original pitch feature #1 — is the least-developed thread. The standing task (compositor DRM + seL4 ping/pong) unblocks both the MVP and the core isolation claim. After that: desktop shell (Phase 2), then security partitions (Phase 3), then AI + polish (Phase 4).

---

*This roadmap is grounded entirely in `docs/PRD.md`, `docs/ROADMAP.md`, `docs/ARCHITECTURE.md`, and the 23 specs in `docs/specs/`. Status assessments are honest — verified means real QEMU/serial/test evidence; code-level means host-sim tests pass but real integration pending; not-started means spec only.*
