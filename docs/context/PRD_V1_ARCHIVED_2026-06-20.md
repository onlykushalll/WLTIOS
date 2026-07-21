# [ARCHIVED — SUPERSEDED 2026-07-21] WLTIOS Product Requirements Document

> **This document is stale.** It assumed a traditional 24-month, week-by-week
> waterfall pace starting 2026-06-20. Actual velocity has been dramatically
> faster on some tracks (Alpha-scoped features like compositor, window
> management, taskbar, launcher, settings, and package manager were real
> and verified within about a month, not the 6 months this doc planned) and
> hit real unexpected walls on others (TorPD/seL4 multi-PD work). The AI
> model default listed here (Llama-Vision) has also been superseded.
>
> The current living PRD is at `docs/PRD.md`. Kept here for history only —
> do not treat any status or timeline claim below as current.

---

# WLTIOS — Product Requirements Document (PRD)

> **Product Name:** WLTIOS (pronounced L-TOS)
> **Version:** 1.0 PRD
> **Date:** 2026-06-20
> **Document Owner:** WLTIOS Team
> **Status:** Approved for Development

---

## 1. Product Vision

### Vision Statement
WLTIOS is a general-purpose desktop operating system that combines the usability of Windows, the power and hardware support of Linux, the anonymity of Tails, and the verified security of seL4 — with AI natively integrated at the OS level.

### Target User
- **Primary:** Privacy-conscious power users who want a daily-driver OS
- **Secondary:** Developers who need a flexible, scriptable, secure environment
- **Tertiary:** Journalists, activists, researchers who need anonymity + usability

### Problem We Solve
No existing OS combines all of these:
- Familiar, user-friendly GUI (Windows has this)
- Low resource usage, hardware support (Linux has this)
- Strong anonymity guarantees (Tails has this)
- Mathematical security isolation (seL4 has this, but no consumer OS uses it)
- Built-in AI assistance (nobody has this natively)

**Users currently must choose between usability, security, anonymity, and AI. WLTIOS eliminates the compromise.**

---

## 2. Product Goals

| # | Goal | How We Measure Success |
|---|------|----------------------|
| G1 | **Bootable, daily-driver OS** | Can replace Windows/Linux for typical use cases |
| G2 | **Anonymous by default** | All traffic through Tor, no leaks, amnesic option |
| G3 | **Fast and lightweight** | <256MB RAM idle, <3s boot, <2GB disk |
| G4 | **Secure by design** | seL4 isolation, verified boot, app sandboxing |
| G5 | **AI-native** | Built-in AI assistant that sees, hears, and helps |
| G6 | **Familiar UX** | Users from Windows/Linux feel at home in minutes |
| G7 | **Open source** | Full source available, community contributions welcome |

---

## 3. Non-Goals (What We Are NOT Building)

| Non-Goal | Why |
|----------|-----|
| A server OS | Focus on desktop/laptop; servers have different needs |
| A mobile OS | Desktop first; mobile is a future possibility |
| Windows app compatibility | Not trying to run .exe files (Wine is separate project) |
| A Linux distro | We're building a full OS, not repackaging Linux |
| 100% from scratch | We reuse Linux kernel + seL4 (pragmatic, not pure) |
| Backwards compatibility with legacy systems | Clean slate; modern hardware only (UEFI, x86-64) |

---

## 4. Functional Requirements

### 4.1 Boot System (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-BOOT-01 | Boot from UEFI firmware | P0 |
| FR-BOOT-02 | Verified boot (signature check) | P0 |
| FR-BOOT-03 | Boot time < 3 seconds | P1 |
| FR-BOOT-04 | Boot from USB (Live mode) | P0 |
| FR-BOOT-05 | Boot from internal disk (Installed mode) | P1 |
| FR-BOOT-06 | Recovery mode if boot fails | P1 |
| FR-BOOT-07 | Dual-boot support (with Windows/Linux) | P2 |

### 4.2 Kernel + Isolation (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-KERN-01 | seL4 microkernel as verified base | P0 |
| FR-KERN-02 | Linux kernel as guest (for hardware support) | P0 |
| FR-KERN-03 | Tor daemon in seL4 partition | P0 |
| FR-KERN-04 | Key vault in seL4 partition | P0 |
| FR-KERN-05 | Firewall in seL4 partition | P0 |
| FR-KERN-06 | Boot verifier in seL4 partition | P1 |
| FR-KERN-07 | Auth service in seL4 partition | P1 |

### 4.3 Graphical User Interface (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-GUI-01 | Custom Wayland compositor | P0 |
| FR-GUI-02 | Desktop with taskbar/dock | P0 |
| FR-GUI-03 | Window management (move, resize, close, minimize) | P0 |
| FR-GUI-04 | Window snapping (half, quarter, full screen) | P1 |
| FR-GUI-05 | Alt+Tab app switcher | P0 |
| FR-GUI-06 | App launcher (search + launch) | P0 |
| FR-GUI-07 | System tray (network, battery, clock, Tor status) | P1 |
| FR-GUI-08 | Notifications system | P1 |
| FR-GUI-09 | Dark/light theme | P1 |
| FR-GUI-10 | Custom widget toolkit (buttons, menus, dialogs) | P0 |
| FR-GUI-11 | Right-click context menus | P0 |
| FR-GUI-12 | Drag and drop | P1 |
| FR-GUI-13 | Multi-monitor support | P2 |

### 4.4 Core Applications (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-APP-01 | Terminal emulator | P0 |
| FR-APP-02 | File manager (with Tor download + encryption) | P0 |
| FR-APP-03 | Text editor | P0 |
| FR-APP-04 | Settings app | P0 |
| FR-APP-05 | Task manager (process viewer) | P1 |
| FR-APP-06 | Screenshot tool | P1 |
| FR-APP-07 | Calculator | P1 |
| FR-APP-08 | Image viewer | P2 |
| FR-APP-09 | Media player (audio/video) | P2 |
| FR-APP-10 | Web browser (Tor Browser-based) | P0 |

### 4.5 Anonymity + Privacy (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-ANON-01 | All network traffic through Tor | P0 |
| FR-ANON-02 | MAC address randomization on boot | P0 |
| FR-ANON-03 | Kill switch (block all if Tor dies) | P0 |
| FR-ANON-04 | DNS through Tor (no leaks) | P0 |
| FR-ANON-05 | Amnesic mode (RAM-only, no disk writes) | P0 |
| FR-ANON-06 | Encrypted persistence (optional) | P1 |
| FR-ANON-07 | No telemetry, ever | P0 |
| FR-ANON-08 | Secure delete (shred files on deletion) | P1 |
| FR-ANON-09 | Compartmentalized identities | P2 |
| FR-ANON-10 | Tor circuit viewer (see your route) | P1 |

### 4.6 Security (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-SEC-01 | App sandboxing (cgroups + namespaces) | P0 |
| FR-SEC-02 | App permission system (network, files, camera) | P0 |
| FR-SEC-03 | Disk encryption (LUKS) | P0 |
| FR-SEC-04 | File encryption (per-file) | P1 |
| FR-SEC-05 | Verified boot (kernel signature check) | P0 |
| FR-SEC-06 | Privacy badges on apps (Tor-only? camera access?) | P1 |
| FR-SEC-07 | Security audit log | P1 |
| FR-SEC-08 | Firewall config UI | P1 |

### 4.7 Package Management (P1 — Important)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-PKG-01 | Install packages from repository | P0 |
| FR-PKG-02 | Remove packages | P0 |
| FR-PKG-03 | Update packages | P0 |
| FR-PKG-04 | Atomic updates (all-or-nothing) | P0 |
| FR-PKG-05 | Rollback to previous version | P1 |
| FR-PKG-06 | Signed packages (GPG) | P0 |
| FR-PKG-07 | Delta updates (only download changes) | P2 |
| FR-PKG-08 | Offline install from cache | P1 |

### 4.8 AI Assistant (P1 — Important)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-AI-01 | Built-in AI assistant (sidebar) | P1 |
| FR-AI-02 | Screen understanding (VLM) | P1 |
| FR-AI-03 | Voice input (speech-to-text) | P2 |
| FR-AI-04 | Voice output (text-to-speech) | P2 |
| FR-AI-05 | System control (open apps, change settings) | P1 |
| FR-AI-06 | File search ("find my PDF from last week") | P1 |
| FR-AI-07 | Privacy analysis ("which apps are spying?") | P2 |
| FR-AI-08 | Runs locally by default (no cloud required) | P1 |
| FR-AI-09 | Optional cloud model support | P2 |

### 4.9 Network (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FR-NET-01 | WiFi connection management | P0 |
| FR-NET-02 | Ethernet connection management | P0 |
| FR-NET-03 | VPN support (WireGuard) | P1 |
| FR-NET-04 | Network status indicator | P0 |
| FR-NET-05 | All traffic routed through Tor | P0 |
| FR-NET-06 | Bluetooth support | P2 |

### 4.10 Filesystem (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| FS-01 | Support ext4 filesystem | P0 |
| FS-02 | Support btrfs (for snapshots) | P1 |
| FS-03 | LUKS disk encryption | P0 |
| FS-04 | Read-only root filesystem | P1 |
| FS-05 | tmpfs for /tmp (RAM disk) | P0 |
| FS-06 | Union filesystem (OverlayFS) | P1 |

### 4.11 Performance (P1 — Important)

| Requirement | Target | Priority |
|-------------|--------|----------|
| PERF-01 | Boot time < 3 seconds | P1 |
| PERF-02 | Idle RAM < 256MB | P1 |
| PERF-03 | Disk install < 2GB | P1 |
| PERF-04 | App launch < 500ms | P2 |
| PERF-05 | Shutdown < 2 seconds | P2 |

### 4.12 Hardware Support (P0 — Critical)

| Requirement | Description | Priority |
|-------------|-------------|----------|
| HW-01 | x86-64 CPU support | P0 |
| HW-02 | UEFI boot | P0 |
| HW-03 | NVIDIA GPU (via Linux drivers) | P1 |
| HW-04 | AMD GPU (via Linux drivers) | P1 |
| HW-05 | Intel GPU (via Linux drivers) | P0 |
| HW-06 | WiFi (via Linux drivers) | P0 |
| HW-07 | Ethernet (via Linux drivers) | P0 |
| HW-08 | USB (via Linux drivers) | P0 |
| HW-09 | NVMe/SATA storage (via Linux drivers) | P0 |
| HW-10 | Audio (via Linux drivers) | P1 |
| HW-11 | Bluetooth (via Linux drivers) | P2 |
| HW-12 | Touchpad/trackpad | P1 |
| HW-13 | Multi-monitor | P2 |
| HW-14 | ARM64 architecture | P3 (future) |

---

## 5. Non-Functional Requirements

### 5.1 Security

| NFR | Requirement |
|-----|-------------|
| NFR-SEC-01 | Critical services (Tor, keys, firewall) must run in seL4 partitions, isolated from Linux |
| NFR-SEC-02 | All packages must be signed |
| NFR-SEC-03 | Boot process must verify signatures |
| NFR-SEC-04 | No telemetry or background data collection |
| NFR-SEC-05 | All network traffic must go through Tor (no exceptions) |

### 5.2 Performance

| NFR | Requirement |
|-----|-------------|
| NFR-PERF-01 | Boot in < 3 seconds on recommended hardware |
| NFR-PERF-02 | Idle RAM usage < 256MB |
| NFR-PERF-03 | Disk install < 2GB |
| NFR-PERF-04 | UI must run at 60fps on recommended hardware |

### 5.3 Reliability

| NFR | Requirement |
|-----|-------------|
| NFR-REL-01 | Updates must be atomic (A/B partition, rollback) |
| NFR-REL-02 | Boot must auto-rollback if update fails |
| NFR-REL-03 | Filesystem corruption should be impossible (read-only root + journaling) |
| NFR-REL-04 | Kill switch must trigger in < 100ms if Tor disconnects |

### 5.4 Usability

| NFR | Requirement |
|-----|-------------|
| NFR-USE-01 | Windows user can learn the OS in < 10 minutes |
| NFR-USE-02 | All settings accessible via GUI (no terminal required for basics) |
| NFR-USE-03 | Terminal available for power users |
| NFR-USE-04 | Keyboard shortcuts match common expectations (Ctrl+C, Ctrl+V, Alt+Tab) |

### 5.5 Privacy

| NFR | Requirement |
|-----|-------------|
| NFR-PRIV-01 | Zero data leaves device without explicit user action |
| NFR-PRIV-02 | No crash reports sent automatically |
| NFR-PRIV-03 | No usage analytics |
| NFR-PRIV-04 | MAC address randomized on every boot |
| NFR-PRIV-05 | Amnesic mode leaves no trace on hardware |

---

## 6. User Personas

### Persona 1: "Privacy Paradox" User (Primary)
- **Name:** Alex
- **Age:** 28
- **Occupation:** Software developer
- **Pain:** Wants privacy but finds Tails too limited for daily use. Uses Windows for convenience but worries about surveillance.
- **Needs:** An OS that's both private AND usable. Doesn't want to choose.
- **WLTIOS solves:** Daily-driver OS with Tor built-in. No compromise.

### Persona 2: Power User (Secondary)
- **Name:** Jordan
- **Age:** 35
- **Occupation:** System administrator
- **Pain:** Loves Linux but hates the fragmented UX. Wants something polished.
- **Needs:** Power of Linux with UX of Windows/macOS.
- **WLTIOS solves:** Linux under the hood, custom polished GUI on top.

### Persona 3: At-Risk User (Tertiary)
- **Name:** Sam
- **Age:** 40
- **Occupation:** Journalist
- **Pain:** Needs anonymity for source protection. Tails works but is slow and limited.
- **Needs:** Strong anonymity with enough usability to do real work.
- **WLTIOS solves:** Tails-grade anonymity + seL4 security + usable GUI.

---

## 7. Competitive Analysis

| Feature | WLTIOS | Windows 11 | Ubuntu | Tails | Qubes | macOS |
|---------|--------|-----------|--------|-------|-------|-------|
| Familiar GUI | Custom | Native | OK | Minimal | Limited | Native |
| Hardware support | Via Linux | Native | Native | Limited | Limited | Apple only |
| Tor everywhere | Default | No | No | Default | Optional | No |
| Amnesic mode | Default | No | No | Default | No | No |
| Verified security | seL4 | No | No | No | Xen | No |
| App sandboxing | Default | Limited | No | No | Default | Limited |
| Built-in AI | Native | Copilot | No | No | No | Apple Intelligence |
| Low RAM (<512MB) | Yes | No (4GB+) | Partial (1GB+) | Partial (800MB+) | No (4GB+) | No (4GB+) |
| Open source | Yes | No | Yes | Yes | Yes | No |

---

## 8. Dependencies

### External Dependencies

| Dependency | Purpose | Risk |
|------------|---------|------|
| seL4 | Verified microkernel | Low — stable, maintained |
| Linux kernel | Hardware support | Low — very stable |
| wlroots | Wayland implementation | Medium — actively developed |
| musl libc | C library | Low — stable |
| Mesa | GPU drivers | Low — stable |
| Tor | Anonymity network | Low — stable |
| Rust | System language | Low — stable |

### No External Dependencies For

- Bootloader (custom)
- Init system (custom)
- Compositor (custom, built on wlroots)
- Widget toolkit (custom)
- Package manager (custom)
- Security layer (custom)

---

## 9. Glossary

| Term | Definition |
|------|-----------|
| **seL4** | A formally verified microkernel — mathematically proven to have no bugs |
| **Wayland** | Modern display protocol (replaces X11) |
| **wlroots** | Library implementing Wayland compositor functionality |
| **Tor** | The Onion Router — anonymizes network traffic through 3-hop relays |
| **LUKS** | Linux Unified Key Setup — disk encryption standard |
| **cgroups** | Control groups — Linux feature for limiting process resources |
| **namespaces** | Linux feature for isolating processes from each other |
| **tmpfs** | Filesystem in RAM (lost on reboot) |
| **MVP** | Minimum Viable Product — simplest useful version |
| **A/B partition** | Two OS partitions — update one while running the other |
| **Amnesic** | Forgets everything on reboot (RAM-only) |
| **Kill switch** | Blocks all network traffic if Tor disconnects |

---

*Original document. Release-phase timeline and success-metrics sections have
been superseded by docs/PRD.md's track-based structure and are omitted here
— see the archived ROADMAP for the original month-by-month plan.*
