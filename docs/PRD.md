# WLTIOS — Product Requirements Document

> **Version:** 2.0 (supersedes the 2026-06-20 original, archived at
> `docs/context/PRD_V1_ARCHIVED_2026-06-20.md`)
> **Last updated:** 2026-07-21
> **Maintained by:** Claude (planner/instructor role) — updated after each
> verified build phase, not before. This is the source of truth for scope
> and sequencing; individual build prompts should pull from this document,
> not improvise parallel plans.
> **Status:** Living document

---

## 1. Product Vision

WLTIOS is a custom desktop operating system that combines seL4 microkernel
isolation, Linux as a guest VM (for hardware support), Tor-based anonymity,
a Wayland compositor, a native AI assistant (Kairos), and a security
monitoring engine (Guardian) — built to be genuinely secure by architecture
rather than by policy, efficient enough for modest hardware, broadly usable
day to day, and visually distinctive rather than looking like a stock Linux
desktop.

### Target User
- **Primary:** Privacy-conscious power users who want a daily-driver OS, not just a boot-and-browse tool
- **Secondary:** Developers who want a flexible, scriptable, secure environment
- **Tertiary:** Journalists, activists, researchers who need real anonymity plus enough usability to do real work

### Problem Being Solved
No existing OS combines all of: a familiar, polished GUI (Windows has this); low resource usage and broad hardware support (Linux has this); strong anonymity guarantees (Tails has this); mathematically verified security isolation (seL4 has this, but no consumer OS uses it); and built-in, tool-using AI assistance (nobody has this natively). Users currently choose between usability, security, anonymity, and AI capability. WLTIOS is trying to eliminate that tradeoff.

---

## 2. Product Pillars (translating the four reference points into build targets)

| Pillar (as originally stated) | What it actually means as a build target |
|---|---|
| "Everything Windows has" | **Scoped as:** broad daily-driver usability — doesn't feel limited for common tasks. **Not** literal Windows app compatibility (`.exe` execution) — this was correctly scoped out from day one (see Non-Goals) and remains out of scope; it's an open-ended ecosystem-scale undertaking, not a bounded backlog item. |
| "Tails-like security, actually" | Real seL4 multi-partition isolation (not just sandboxing inside one Linux guest) + Tor-by-default + amnesia/kill-switch that wipes kernel-space secrets, not just userspace ones. The architecture is a boundable engineering task; "actually Tails-level" as a *track record* claim is something time and real-world scrutiny earn after 1.0 ships, not something more build sessions alone can produce. |
| "Linux-like low RAM and functionality" | Largely inherent — this is a custom Linux-based stack by construction. Mostly a constraint to protect (don't let RAM creep) rather than a feature to build fresh. |
| "Apple-like UI — glassmorphism/liquid glass" | Real GPU-accelerated compositor rendering (current: software/pixman) + backdrop blur + a written design system + consistent application across every UI surface. New work, not yet started, comparable in scope to the desktop shell work already completed. |

---

## 3. Non-Goals (unchanged from the original PRD — still correct)

| Non-Goal | Why |
|----------|-----|
| A server OS | Focus is desktop/laptop; servers have different needs |
| A mobile OS | Desktop first; mobile is a possible future project, not this one |
| Windows app compatibility (`.exe` execution) | Not attempting Wine-equivalent compatibility — a separate, open-ended undertaking |
| A repackaged Linux distro | Building a full custom OS (custom boot, init, compositor, security layer), not repackaging existing Linux desktop components |
| 100% from scratch | Reuses the Linux kernel (for hardware support) and seL4 (for isolation) — pragmatic, not purist |
| Legacy hardware support | Modern hardware only (UEFI, x86-64) |

---

## 4. Current Verified State (as of 2026-07-21)

This section reflects what's been independently verified against the actual
repository, not what any status document claims. Prior status docs in this
repo have been found materially wrong before (one called roughly 7,000 lines
of real, working code "not started") — treat any status claim, including
this one, as something to re-verify against the repo rather than trust on
its own, especially as time passes since the "last updated" date above.

### Solid and verified
- seL4 boots; 64-bit Linux guest VM boots; Key Vault PD verified with real crypto ping/pong across the seL4 partition boundary
- Wayland compositor with real window management: resize, minimize, maximize, click-to-focus, drag-to-move, Alt+Tab
- Desktop shell: terminal, file manager, app launcher, taskbar, settings app — all real, all rendering simultaneously in one session
- Kairos AI wired in-process (previously shelled raw user input via `fork()`+`execl("/bin/sh"...)` — a real command-injection path, now closed); real keyboard input handling; calls `wlt_ai_query` directly, no external process
- `wlt-permissions` enforcement wired into `wlt-sandbox/launch.c`; both the allow and deny branches proven with a real automated test harness
- Real networking (e1000 driver added to kernel config); Tor reported to bootstrap to 100% in QEMU (per build report — not yet independently re-verified live by Claude, since Claude's WSL2 execution bridge has been non-functional for direct verification this project)
- Guardian FIM fixed (was failing on a relative-path config bug; fixed via `guardian.conf` with absolute paths)
- `wlt-pkg` real install/remove lifecycle proven for one test package (install → verify → remove, real filesystem operations, real dependency tracking — not a hardcoded stub)
- Reproducibility: Tor's runtime libs and wlroots/wayland libs are vendored properly under `packages/` and survive clean rebuilds (independently verified at the source level)

### Real but unproven or partial
- **TorPD:** component scaffold builds and runs (`[tor-pd] alive` in the serial log), but the real IPC round-trip is blocked by a 32-bit seL4 memory ceiling. The design doc's own decision (TorPD as a second Linux guest VM) was never actually attempted — what got built was a bare-component copy of Key Vault PD's pattern instead, which the design doc itself says won't work for something that needs glibc/sockets/filesystem/threads like Tor does. Currently being re-approached via a 64-bit port of Key Vault PD first, to de-risk before retrying TorPD for real.
- **AI pipeline:** proven end-to-end against a mock LLM server (real HTTP round-trip, real response rendering in the Kairos window) — never tested against a real model.
- **Disk encryption:** audit phase just started.

### Not started
- Firewall PD, Boot Verifier PD (two of the three planned seL4 security partitions)
- Real package repository (currently one local test package proven; no hosting, no real signing infrastructure beyond a single proof)
- Tor Browser integration
- GPU-accelerated compositor rendering; any glassmorphism/design-system UI work
- Real hardware validation (deliberately deferred — no second OS boot wanted on the daily-driver PC; QEMU is the only proving ground for now)
- Voice/hotword (likely needs real hardware mic input to meaningfully test; audit-only for now)

---

## 5. Active Tracks (in flight right now)

Run in parallel deliberately — this has outperformed sequential phases
because nothing blocks on anything unrelated, and each track gets audited
independently before anything gets built on top of it.

1. **seL4 unification + disk encryption** — reconcile the 32-bit (Key Vault/TorPD) vs. 64-bit (desktop) seL4 fork with real evidence; port Key Vault PD to 64-bit first as a de-risking step before retrying TorPD as a real second Linux guest VM. Bundled with real LUKS disk encryption against an actual virtual disk and a kexec-based kernel-space key wipe for real amnesia (not just clearing userspace secrets).
2. **Kairos full feature wiring** — roughly 13 remaining `wlt-ai/` files (vision, anonymity coach, privacy analysis, sidebar, a Guardian-to-Kairos event bridge) that are real, compiling code, never wired into anything reachable by a user.
3. **wlt-update + real package repo** — A/B partition update scheme (needs a design reality check — may not match how this project's boot process actually works yet), libsodium-backed Ed25519 package signing done properly (vendored/committed, not a scratch fix), and a second real installable package to prove the repo concept beyond one item.

## 6. Queued, Not Yet Started

Pull from here when a track above lands. Don't improvise new scope ad hoc.

- **GPU-accelerated compositor + design system** — scoped in detail below (Section 7); the first real step toward the UI pillar.
- **Qwythos-9B AI model verification** — deliberately deferred until a bootable Debian live-USB environment frees enough real RAM to test it properly; timing is an explicit, later decision, not scheduled.
- **Firewall PD + Boot Verifier PD** — blocked on however the seL4 unification track resolves; the same 32-bit/64-bit architecture questions apply to both.
- **Real package repository hosting** (versus the current local-only proof)
- **Tor Browser integration**
- **Real hardware boot validation** — deferred indefinitely; QEMU remains the only proving ground unless this decision changes

## 7. UI / Glassmorphism Track — Detailed Breakdown (queued)

The most-scoped-out queued item, since it's genuinely new territory with no prior work in this project to build on.

- **Phase 1 — Rendering foundation:** check what Mesa/DRI infrastructure already exists from earlier compositor work before assuming from scratch. Get the compositor rendering via GLES2 (or whatever wlroots' pluggable renderer support reveals as the real path) instead of the current pixman/software path. Verify against QEMU's actual virtual GPU ceiling — likely an llvmpipe software fallback — and keep "a real GPU rendering pipeline exists" and "actually GPU-accelerated on real hardware" as distinct claims, not the same thing.
- **Phase 2 — One real blur pass:** a genuine dual-pass backdrop blur shader, proven on one real surface. Verify actual pixel content shows blurred detail, not alpha blending dressed up as blur.
- **Phase 3 — Design system spec:** written down before more surfaces get built ad hoc. Every UI client so far (terminal, file manager, settings, statusbar, launcher, Kairos) was built independently with no shared visual language — this is where that stops.
- **Phase 4 — Apply to one real surface fully** (taskbar or Kairos sidebar are the highest-visibility candidates), then propagate to the rest of the desktop in later phases once the pattern is proven.

---

## 8. Functional Requirements

Original target requirements from the v1 PRD, largely still valid as the
feature backlog. Priority (P0/P1/P2) reflects original target importance,
not current build status — see Section 4 above for what's actually verified
done. Treat this table as "what we're building toward," and Section 4 as
"what's actually true right now."

### 8.1 Boot System
| Requirement | Description | Priority |
|---|---|---|
| FR-BOOT-01 | Boot from UEFI firmware | P0 |
| FR-BOOT-02 | Verified boot (signature check) | P0 |
| FR-BOOT-03 | Boot time < 3 seconds | P1 |
| FR-BOOT-04 | Boot from USB (Live mode) | P0 |
| FR-BOOT-05 | Boot from internal disk (Installed mode) | P1 |
| FR-BOOT-06 | Recovery mode if boot fails | P1 |
| FR-BOOT-07 | Dual-boot support (Windows/Linux) | P2 |

### 8.2 Kernel + Isolation
| Requirement | Description | Priority |
|---|---|---|
| FR-KERN-01 | seL4 microkernel as verified base | P0 — done |
| FR-KERN-02 | Linux kernel as guest for hardware support | P0 — done |
| FR-KERN-03 | Tor daemon in its own seL4 partition | P0 — in progress (TorPD) |
| FR-KERN-04 | Key vault in seL4 partition | P0 — done |
| FR-KERN-05 | Firewall in seL4 partition | P0 — not started |
| FR-KERN-06 | Boot verifier in seL4 partition | P1 — not started |
| FR-KERN-07 | Auth service in seL4 partition | P1 — not started |

### 8.3 Graphical User Interface
| Requirement | Description | Priority |
|---|---|---|
| FR-GUI-01 | Custom Wayland compositor | P0 — done |
| FR-GUI-02 | Desktop with taskbar/dock | P0 — done |
| FR-GUI-03 | Window management (move, resize, close, minimize) | P0 — done |
| FR-GUI-04 | Window snapping (half, quarter, full screen) | P1 — not started |
| FR-GUI-05 | Alt+Tab app switcher | P0 — done |
| FR-GUI-06 | App launcher (search + launch) | P0 — done |
| FR-GUI-07 | System tray (network, battery, clock, Tor status) | P1 — partial (statusbar shows real data, no click actions) |
| FR-GUI-08 | Notifications system | P1 — not started |
| FR-GUI-09 | Dark/light theme | P1 — not started |
| FR-GUI-10 | Custom widget toolkit | P0 — ad hoc per-app so far, no shared toolkit yet |
| FR-GUI-11 | Right-click context menus | P0 — not started |
| FR-GUI-12 | Drag and drop | P1 — not started |
| FR-GUI-13 | Multi-monitor support | P2 — not started |
| FR-GUI-14 | Glassmorphism / backdrop blur / design system | P1 — not started (see Section 7) |

### 8.4 Core Applications
| Requirement | Description | Priority |
|---|---|---|
| FR-APP-01 | Terminal emulator | P0 — done |
| FR-APP-02 | File manager | P0 — done |
| FR-APP-03 | Text editor | P0 — not started |
| FR-APP-04 | Settings app | P0 — done |
| FR-APP-05 | Task manager | P1 — not started |
| FR-APP-06 | Screenshot tool | P1 — not started |
| FR-APP-07 | Calculator | P1 — not started |
| FR-APP-08 | Image viewer | P2 — not started |
| FR-APP-09 | Media player | P2 — not started |
| FR-APP-10 | Web browser (Tor Browser-based) | P0 — not started |

### 8.5 Anonymity + Privacy
| Requirement | Description | Priority |
|---|---|---|
| FR-ANON-01 | All network traffic through Tor | P0 — real networking + Tor bootstrap done, full traffic routing not yet verified |
| FR-ANON-02 | MAC address randomization on boot | P0 — done |
| FR-ANON-03 | Kill switch | P0 — done |
| FR-ANON-04 | DNS through Tor, no leaks | P0 — not independently verified |
| FR-ANON-05 | Amnesic mode (RAM-only) | P0 — kexec-based real kernel-space wipe in progress |
| FR-ANON-06 | Encrypted persistence (optional) | P1 — disk encryption audit in progress |
| FR-ANON-07 | No telemetry, ever | P0 — no telemetry code exists, holds by default |
| FR-ANON-08 | Secure delete | P1 — not started |
| FR-ANON-09 | Compartmentalized identities | P2 — not started |
| FR-ANON-10 | Tor circuit viewer | P1 — backend real (`wlt-tor-mgr`/`circuits.c` verified real), no UI wired yet |

### 8.6 Security
| Requirement | Description | Priority |
|---|---|---|
| FR-SEC-01 | App sandboxing (cgroups + namespaces) | P0 — real, wired into launch.c |
| FR-SEC-02 | App permission system | P0 — done, both branches tested |
| FR-SEC-03 | Disk encryption (LUKS) | P0 — in progress |
| FR-SEC-04 | File encryption (per-file) | P1 — not started |
| FR-SEC-05 | Verified boot | P0 — not started |
| FR-SEC-06 | Privacy badges on apps | P1 — not started |
| FR-SEC-07 | Security audit log | P1 — real (`wlt-permissions/audit.c` verified) |
| FR-SEC-08 | Firewall config UI | P1 — not started (no Firewall PD yet either) |

### 8.7 Package Management
| Requirement | Description | Priority |
|---|---|---|
| FR-PKG-01 | Install from repository | P0 — real for one local test package |
| FR-PKG-02 | Remove packages | P0 — done |
| FR-PKG-03 | Update packages | P0 — not started |
| FR-PKG-04 | Atomic updates | P0 — A/B design in progress |
| FR-PKG-05 | Rollback | P1 — in progress |
| FR-PKG-06 | Signed packages | P0 — in progress (libsodium/Ed25519) |
| FR-PKG-07 | Delta updates | P2 — not started |
| FR-PKG-08 | Offline install from cache | P1 — not started |

### 8.8 AI Assistant
| Requirement | Description | Priority |
|---|---|---|
| FR-AI-01 | Built-in AI assistant (sidebar) | P1 — window done, true always-on sidebar in progress |
| FR-AI-02 | Screen understanding (VLM) | P1 — code real, not yet wired (in progress) |
| FR-AI-03 | Voice input (STT) | P2 — audit-only, needs real hardware to test |
| FR-AI-04 | Voice output (TTS) | P2 — audit-only |
| FR-AI-05 | System control | P1 — real tool implementations exist (`tools.c`) |
| FR-AI-06 | File search | P1 — real (`file_search.c` verified) |
| FR-AI-07 | Privacy analysis | P2 — in progress |
| FR-AI-08 | Runs locally by default | P1 — done (Qwen2.5-1.5B baseline proven; Qwythos-9B queued) |
| FR-AI-09 | Optional cloud model support | P2 — stubbed only |

### 8.9 Network
| Requirement | Description | Priority |
|---|---|---|
| FR-NET-01 | WiFi management | P0 — not started |
| FR-NET-02 | Ethernet management | P0 — real (e1000) |
| FR-NET-03 | VPN support (WireGuard) | P1 — not started |
| FR-NET-04 | Network status indicator | P0 — real data in statusbar |
| FR-NET-05 | All traffic through Tor | P0 — see 8.5 |
| FR-NET-06 | Bluetooth | P2 — not started |

### 8.10 Filesystem
| Requirement | Description | Priority |
|---|---|---|
| FS-01 | ext4 support | P0 — inherited from Linux guest |
| FS-02 | btrfs support | P1 — not started |
| FS-03 | LUKS disk encryption | P0 — in progress |
| FS-04 | Read-only root | P1 — not started |
| FS-05 | tmpfs for /tmp | P0 — inherited from Linux guest |
| FS-06 | OverlayFS | P1 — not started |

### 8.11 Hardware Support
Unchanged from original — target is x86-64/UEFI, GPU/WiFi/audio/etc. via Linux drivers where the Linux guest has hardware access. Real hardware validation deliberately deferred (Section 6).

---

## 9. Non-Functional Requirements

Original targets, unchanged, still the right bar:
- **Security:** critical services isolated in seL4 partitions; all packages signed; boot verifies signatures; no telemetry; all network traffic through Tor.
- **Performance:** boot < 3s; idle RAM < 256MB; disk install < 2GB; UI at 60fps on target hardware.
- **Reliability:** atomic updates with auto-rollback; kill switch < 100ms.
- **Usability:** a Windows/Linux user productive within minutes; GUI-first with terminal available for power users.
- **Privacy:** zero data leaves the device without explicit action; no analytics or crash telemetry; MAC randomized every boot; amnesic mode leaves no trace.

None of these have been benchmarked/verified yet at the system level — they remain targets, not measured results.

---

## 10. User Personas

*(Unchanged from the original PRD — still accurate.)*

- **Alex (primary), 28, software developer** — wants privacy but finds Tails too limited for daily use; uses Windows for convenience but worries about surveillance. WLTIOS solves this with a daily-driver OS with Tor built in — no compromise required.
- **Jordan (secondary), 35, sysadmin** — loves Linux but hates the fragmented UX; wants something polished. WLTIOS solves this with Linux under the hood and a custom, polished GUI on top.
- **Sam (tertiary), 40, journalist** — needs anonymity for source protection; Tails works but is slow and limited. WLTIOS solves this with Tails-grade anonymity plus seL4 security plus a genuinely usable GUI.

---

## 11. Competitive Analysis

| Feature | WLTIOS (target) | Windows 11 | Ubuntu | Tails | Qubes | macOS |
|---|---|---|---|---|---|---|
| Familiar GUI | Custom | Native | OK | Minimal | Limited | Native |
| Hardware support | Via Linux | Native | Native | Limited | Limited | Apple only |
| Tor everywhere | Default | No | No | Default | Optional | No |
| Amnesic mode | Default | No | No | Default | No | No |
| Verified security | seL4 | No | No | No | Xen | No |
| App sandboxing | Default | Limited | No | No | Default | Limited |
| Built-in AI | Native | Copilot | No | No | No | Apple Intelligence |
| Low RAM (<512MB) | Target | No | Partial | Partial | No | No |
| Open source | Yes | No | Yes | Yes | Yes | No |

---

## 12. Working Methodology

Stated explicitly so it doesn't stay implicit:
- **Parallel independent tracks**, not sequential phases — maximizes real progress per session; nothing waits on anything unrelated.
- **Audit before building** — every "orphaned code" pass has found real, sizable code sitting disconnected from anything reachable; check what exists before writing more.
- **Every build report gets independently verified against the actual repo** before anything is called done — this has caught real issues nearly every round: inflated line counts, commit messages that don't match their diffs, claims proven only in a scratch environment that never made it into the tracked repo, a design doc contradicted by what was actually built. This isn't distrust for its own sake — it's what keeps "done" meaningful on a project long enough that no one person holds the whole state in their head.
- **Reproducibility is the actual bar** — "compiles," "boots," or "a function returned success" is not "done." Something is done when it survives a clean rebuild from the tracked repo alone.
- **Honest partial results are a legitimate outcome** — TorPD hitting a real architectural wall was reported straight rather than compressed into a false "done," and that's the standard going forward, not an exception.

---

## 13. Timeline (a living estimate, not a promise)

- **Core security value proposition working** (desktop, AI assistant, disk encryption with real amnesia, Key Vault PD, real package install) — realistic at the pace the active tracks have moved so far: **mid-to-late August 2026**.
- **Full vision as scoped in this document** (all three seL4 partitions, real package repo, Tor Browser, the UI pillar fully applied) — **4-6 months** from 2026-07-21. The width of that range is mostly explained by one real unknown: whether Firewall PD and Boot Verifier PD go smoothly (like the permissions/networking tracks did) or hit a wall (like TorPD did) — there's no evidence either way yet, and it will narrow as the seL4 unification track resolves.
- This section updates as tracks land — it's a forecast built from observed pace, not a fixed deadline.

---

## 14. Dependencies

| Dependency | Purpose | Risk |
|---|---|---|
| seL4 | Verified microkernel | Low — stable, maintained |
| Linux kernel | Hardware support | Low — very stable |
| wlroots | Wayland compositor library | Medium — actively developed |
| musl libc | Primary C library (rootfs) | Low — stable |
| glibc | Vendored for Tor + some components that need it | Low — stable, now vendored reproducibly |
| Mesa | GPU drivers, needed for Section 7's rendering work | Low — stable |
| Tor | Anonymity network | Low — stable |
| libsodium | Ed25519 package signing (in progress) | Low — stable |

No external dependency for: bootloader, init system, compositor (custom, built on wlroots), package manager, security/sandboxing layer, Kairos AI integration layer.

---

## 15. Glossary

| Term | Definition |
|---|---|
| **seL4** | A formally verified microkernel — mathematically proven memory-safe |
| **PD** | Protection Domain — an isolated seL4 partition (e.g. Key Vault PD, TorPD) |
| **Wayland / wlroots** | Modern display protocol / the library this project's compositor is built on |
| **Tor** | The Onion Router — anonymizes traffic through multi-hop relays |
| **LUKS** | Linux Unified Key Setup — disk encryption standard |
| **cgroups / namespaces** | Linux kernel features for resource limiting / process isolation |
| **kexec** | Linux mechanism to boot directly into a new kernel without a full hardware reboot — used here for real kernel-space secret wipe |
| **A/B partition** | Two OS partitions — update one while running the other, for atomic updates + rollback |
| **Amnesic** | Leaves no trace after reboot/wipe |
| **Kill switch** | Blocks all network traffic if Tor disconnects |
| **Kairos** | This project's built-in AI assistant |
| **Guardian** | This project's security monitoring engine (FIM, YARA, BPF hooks) |
