# WLTIOS — Architecture Document

> **Pronounced:** L-TOS
> **Etymology:** **W**indows + **L**inux + **T**ails + **I**nspired **OS**
> **Version:** 1.0 (Architecture Blueprint)
> **Date:** 2026-06-20
> **Status:** Planning → Implementation Ready

---

## 1. Executive Summary

WLTIOS is a general-purpose desktop operating system that combines:

- **Windows-inspired UX** — familiar, flexible, user-friendly interface
- **Linux-powered foundation** — proven kernel, unmatched hardware support, Unix philosophy
- **Tails-grade anonymity** — Tor everywhere, amnesic by default, MAC spoofing, kill switch
- **seL4 verified isolation** — mathematical security guarantees for critical services
- **AI-native integration** — built-in AI assistant at the OS level (not an app)

**No existing OS combines all five.** This is WLTIOS's unique value proposition.

---

## 2. Design Principles

| # | Principle | What It Means |
|---|-----------|--------------|
| 1 | **Everything is a file** | All config, state, and hardware accessible as text files (Unix philosophy) |
| 2 | **Secure by default** | Tor on, telemetry off, disk encrypted, no root login |
| 3 | **Minimal footprint** | <256MB RAM idle, <3s boot, <2GB disk install |
| 4 | **Formal isolation** | Critical services run on seL4, not Linux — mathematically separated |
| 5 | **AI-native** | AI assistant is a first-class OS citizen, not a bolted-on app |
| 6 | **No telemetry, ever** | Zero data leaves the device unless the user explicitly sends it |
| 7 | **Amnesic option** | Can run entirely in RAM, leaving no trace on hardware |
| 8 | **Familiar but better** | UX concepts people know (taskbar, windows, settings) but improved |
| 9 | **Atomic everything** | Updates, installs, and rollbacks are atomic (Nix-style) |
| 10 | **Swappable identity** | Multiple personas/identities for different activities |

---

## 3. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        WLTIOS ARCHITECTURE                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              USER APPLICATIONS LAYER                      │  │
│  │  Browser │ Editor │ Media Player │ Terminal │ File Manager │  │
│  │  AI Assistant │ Settings │ Task Manager │ Package Manager  │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                      │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │              WLTIOS USERSPACE (custom, ~3M lines)         │  │
│  │                                                           │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐   │  │
│  │  │ GUI Layer   │  │ System Layer │  │ Security     │   │  │
│  │  │             │  │              │  │ Layer        │   │  │
│  │  │ • Compositor│  │ • Init       │  │ • Sandbox    │   │  │
│  │  │ • Widget kit│  │ • Shell      │  │ • Permissions│   │  │
│  │  │ • Window mgmt│ │ • Package mgr│  │ • Crypto     │   │  │
│  │  │ • Taskbar   │  │ • Service mgr│  │ • Tor mgr    │   │  │
│  │  │ • Launcher  │  │ • Network mgr│  │ • Firewall   │   │  │
│  │  └─────────────┘  └──────────────┘  └──────────────┘   │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                      │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │              LINUX KERNEL (reuse, ~30M lines)             │  │
│  │  Runs as a GUEST on seL4 (paravirtualized via virtio)    │  │
│  │                                                           │  │
│  │  • Hardware drivers (GPU, WiFi, USB, NVMe, audio)        │  │
│  │  • Filesystem drivers (ext4, btrfs, LUKS)                │  │
│  │  • Network stack (TCP/IP, but routed through seL4/Tor)   │  │
│  │  • Process scheduler (CFS)                               │  │
│  │  • cgroups + namespaces (for app sandboxing)             │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                      │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │              seL4 MICROKERNEL (verified, ~10K lines)      │  │
│  │  Mathematically proven: no bugs, no vulnerabilities     │  │
│  │                                                           │  │
│  │  Runs CRITICAL services in isolated partitions:          │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐   │  │
│  │  │ Tor Daemon  │  │ Crypto Keys  │  │ Boot Verifier│   │  │
│  │  │ (network    │  │ (encryption  │  │ (checks sig  │   │  │
│  │  │  isolation) │  │  keys, certs)│  │  before boot)│   │  │
│  │  └─────────────┘  └──────────────┘  └──────────────┘   │  │
│  │  ┌─────────────┐  ┌──────────────┐                      │  │
│  │  │ Firewall    │  │ Auth Service │                      │  │
│  │  │ (packet     │  │ (login,      │                      │  │
│  │  │  filtering) │  │  biometric)  │                      │  │
│  │  └─────────────┘  └──────────────┘                      │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                      │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │              BOOTLOADER (custom, ~5K lines)               │  │
│  │  UEFI bootloader with verified boot                      │  │
│  │  Checks seL4 signature → checks Linux signature → boots  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                      │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │              HARDWARE (CPU, RAM, GPU, NIC, storage)       │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Component Specifications

### 4.1 Bootloader (WLT-Boot)

| Aspect | Specification |
|--------|--------------|
| **Type** | UEFI bootloader (custom, multiboot2-compliant) |
| **Language** | Assembly (boot stub) + C (main logic) |
| **Lines** | ~5,000 |
| **Responsibilities** | UEFI handshake → verify seL4 signature → load seL4 into memory → transfer control |
| **Verified boot** | Checks SHA-256 signatures of seL4 + Linux kernel before booting |
| **Fallback** | If signature fails, drops to recovery shell |
| **Boot time** | <500ms |

### 4.2 seL4 Microkernel Layer

| Aspect | Specification |
|--------|--------------|
| **Base** | seL4 (open source, formally verified) |
| **Lines** | ~10,000 (seL4 core) + ~50,000 (our partitions) |
| **Verification** | Mathematically proven: no buffer overflows, no null derefs, no deadlocks |
| **Partitions** | 5 isolated service partitions (see below) |
| **Communication** | Message-passing (IPC) between partitions — no shared memory |

#### seL4 Partitions (Critical Services)

| Partition | Purpose | Can Linux Touch It? |
|-----------|---------|---------------------|
| **Tor Gateway** | Runs Tor daemon. All network traffic MUST go through here. | ❌ No — isolated |
| **Key Vault** | Stores encryption keys, certificates, passwords. Encrypts/decrypts on request. | ❌ No — isolated |
| **Boot Verifier** | Re-verifies kernel integrity at runtime. Detects rootkits. | ❌ No — isolated |
| **Firewall** | Packet filtering. Enforces "Tor-only" rule. Kill switch. | ❌ No — isolated |
| **Auth Service** | Login, biometric verification, permission grants. | ❌ No — isolated |

**Key insight:** Even if Linux is fully compromised (rootkit installed), these 5 services remain secure. The attacker cannot:
- Disable Tor (it's in a separate partition)
- Steal encryption keys (they're in a separate partition)
- Disable the firewall (it's in a separate partition)
- Fake authentication (it's in a separate partition)

### 4.3 Linux Kernel Layer

| Aspect | Specification |
|--------|--------------|
| **Base** | Linux LTS (long-term support, currently 6.x) |
| **Lines** | ~30M (kernel + drivers — reused, not written by us) |
| **Role** | Runs as a GUEST on seL4 (paravirtualized) |
| **Drivers** | GPU (NVIDIA/AMD/Intel), WiFi, USB, NVMe, audio, Bluetooth |
| **Customizations** | Minimal config, remove unnecessary modules, enable ZRAM |
| **Network** | All traffic routed to seL4 Tor partition (not direct to NIC) |

**Why paravirtualized Linux on seL4:**
- We get Linux's 15M+ lines of hardware drivers for free
- seL4 isolates Linux — if Linux is hacked, critical services are safe
- This is the Qubes model, but with seL4 instead of Xen (more secure)

### 4.4 WLTIOS Userspace (Custom)

This is where 90% of our work goes. ~3M lines of custom code.

#### 4.4.1 GUI Layer

| Component | Purpose | Lines (est.) | Tech |
|-----------|---------|-------------|------|
| **WLT-Compositor** | Wayland compositor (window management, compositing) | 200K | C + wlroots |
| **WLT-Render** | Rendering engine (shadows, blur, animations) | 150K | C + OpenGL/Vulkan |
| **WLT-Widgets** | Custom widget toolkit (buttons, menus, dialogs) | 500K | C + custom rendering |
| **WLT-Shell** | Desktop shell (taskbar, launcher, notifications) | 300K | C + WLT-Widgets |
| **WLT-Settings** | System settings app | 100K | C + WLT-Widgets |
| **WLT-FileManager** | Custom file manager with Tor + AI + encryption | 150K | C + WLT-Widgets |
| **WLT-Terminal** | Terminal emulator | 50K | C + WLT-Widgets |

**GUI Total: ~1.45M lines**

#### 4.4.2 System Layer

| Component | Purpose | Lines (est.) | Tech |
|-----------|---------|-------------|------|
| **WLT-Init** | Custom init system (replaces systemd) | 20K | C |
| **WLT-Pkg** | Package manager (Nix-style, atomic, rollback) | 150K | Rust |
| **WLT-Net** | Network manager (WiFi, Ethernet, VPN config) | 100K | Rust |
| **WLT-Service** | Service manager (start/stop/monitor daemons) | 50K | C |
| **WLT-User** | User/session management | 30K | C |
| **WLT-Update** | Atomic update system (A/B partition swap) | 80K | Rust |

**System Total: ~430K lines**

#### 4.4.3 Security Layer

| Component | Purpose | Lines (est.) | Tech |
|-----------|---------|-------------|------|
| **WLT-Sandbox** | App sandboxing (cgroups + namespaces wrapper) | 100K | Rust |
| **WLT-Crypto** | Disk encryption (LUKS), file encryption | 80K | C + libsodium |
| **WLT-Tor-Mgr** | Tor control panel (circuit switching, identity) | 50K | Rust |
| **WLT-Firewall-Cfg** | Firewall config UI (talks to seL4 partition) | 40K | Rust |
| **WLT-Permissions** | App permission system (like Android but better) | 70K | Rust |
| **WLT-Audit** | Security audit log (what accessed what) | 30K | Rust |

**Security Total: ~370K lines**

#### 4.4.4 AI Layer

| Component | Purpose | Lines (est.) | Tech |
|-----------|---------|-------------|------|
| **WLT-AI-Core** | AI assistant engine (LLM integration, tool use) | 200K | Rust |
| **WLT-AI-UI** | AI interface (sidebar, voice, commands) | 100K | C + WLT-Widgets |
| **WLT-AI-Vision** | Screen understanding (VLM, OCR) | 80K | Rust + Python |
| **WLT-AI-Audio** | Voice input/output (STT, TTS) | 50K | Rust |

**AI Total: ~430K lines**

#### 4.4.5 Core Utilities

| Component | Purpose | Lines (est.) |
|-----------|---------|-------------|
| Coreutils (ls, cp, mv, etc.) | Can reuse from BusyBox or write custom | 50K |
| Shell (bash-compatible) | Can reuse from bash or write custom | 30K |
| Text editor | Simple built-in editor | 20K |
| Process manager | Like Task Manager | 30K |
| Screenshot tool | Screen capture | 10K |
| Media player | Basic audio/video playback | 50K |
| Image viewer | View images | 20K |
| Calculator | Calculator app | 5K |

**Utilities Total: ~215K lines**

### 4.5 Total Line Count

| Layer | Lines | Written By Us? |
|-------|-------|----------------|
| Bootloader | 5K | ✅ Yes |
| seL4 core | 10K | ❌ No (reuse) |
| seL4 partitions | 50K | ✅ Yes |
| Linux kernel | 30M | ❌ No (reuse) |
| GUI layer | 1.45M | ✅ Yes |
| System layer | 430K | ✅ Yes |
| Security layer | 370K | ✅ Yes |
| AI layer | 430K | ✅ Yes |
| Core utilities | 215K | ⚠️ Partial (some reused) |
| **TOTAL (our code)** | **~2.96M** | ✅ |
| **TOTAL (with reused)** | **~33M** | |

**We write ~3M lines. We reuse ~30M lines (Linux kernel + drivers).**

---

## 5. Security Architecture

### 5.1 Threat Model

| Threat | Mitigation |
|--------|-----------|
| **Kernel exploit** (Linux rootkit) | seL4 isolates critical services — kernel hack can't reach Tor/keys |
| **Network surveillance** | All traffic through Tor (3-hop onion). No direct connections. |
| **Disk forensics** | Amnesic mode (RAM-only) + LUKS encryption + secure delete |
| **Malware** | App sandboxing (cgroups + namespaces). No app can access others. |
| **Supply chain attack** | Verified boot + reproducible builds + signed packages |
| **Physical access** | Disk encryption + USB-only boot option + tamper-evident boot |
| **Side-channel** | seL4 provides temporal isolation (partitions can't spy on each other) |
| **Firmware attack** | Boot verifier checks firmware signature (if supported by hardware) |

### 5.2 Permission System

```
Every app has a manifest declaring:
  ├── Network access: tor-only / direct / none
  ├── File access: sandbox / home / full
  ├── Camera/Mic: allowed / denied / ask
  ├── Location: allowed / denied / ask
  ├── Contacts: allowed / denied / ask
  ├── Notifications: allowed / denied
  └── Background: allowed / denied

User sees a privacy badge on every app:
  🟢 Tor-only, sandboxed, no camera
  🟡 Direct network, home access
  🔴 Full access — review recommended
```

### 5.3 Anonymity Guarantees

| Guarantee | How |
|-----------|-----|
| **IP hidden** | All traffic through Tor. Firewall blocks non-Tor. |
| **DNS leak-proof** | DNS routed through Tor (no system DNS) |
| **MAC randomized** | Network interfaces get random MAC on every boot |
| **No telemetry** | Zero background data collection. No crash reports sent. |
| **Amnesic option** | Live USB mode — nothing touches disk unless user saves |
| **Identity isolation** | Different Tor circuits for different apps (compartmentalization) |
| **Kill switch** | If Tor disconnects, ALL network stops instantly |

---

## 6. Performance Architecture

### 6.1 Boot Sequence

```
0ms       Power on
50ms      UEFI firmware loads WLT-Boot
100ms     WLT-Boot verifies seL4 signature
200ms     WLT-Boot loads seL4 into memory
300ms     seL4 starts, initializes partitions
500ms     seL4 loads Linux kernel as guest
800ms     Linux kernel boots (minimal config)
1500ms    WLT-Init starts essential services
2000ms    WLT-Compositor starts, shows desktop
2500ms    Desktop ready, user can interact

Total boot: <3 seconds
```

### 6.2 Memory Budget

| Component | RAM (idle) |
|-----------|-----------|
| seL4 + partitions | 32MB |
| Linux kernel | 64MB |
| WLT-Compositor | 32MB |
| WLT-Shell (taskbar, etc.) | 16MB |
| WLT-Init + services | 16MB |
| Tor daemon | 32MB |
| AI assistant (idle) | 32MB |
| Buffers/cache | 32MB |
| **TOTAL IDLE** | **~256MB** |

### 6.3 Optimization Techniques

| Technique | Impact |
|-----------|--------|
| musl libc (not glibc) | -4.5MB RAM |
| ZRAM (compressed swap in RAM) | 2x effective RAM |
| Lazy service loading | Faster boot, lower idle |
| Read-only root filesystem | Faster I/O, no corruption |
| Prelinked libraries | Faster app launch |
| No background indexing | Saves CPU + disk |
| No telemetry/crash reports | Saves CPU + network |
| Wayland (not X11) | -50MB RAM |
| Custom init (not systemd) | -100MB RAM, 10x faster boot |

---

## 7. Filesystem Layout

```
/                           Root
├── boot/                   Bootloader, kernel, initramfs
│   ├── WLT-Boot            Custom bootloader
│   ├── sel4.bin            seL4 image
│   ├── linux.img           Linux kernel image
│   └── initramfs.img       Initial RAM filesystem
├── system/                 Read-only system files (WLT packages)
│   ├── bin/                System binaries
│   ├── lib/                System libraries
│   ├── share/              Shared data (icons, fonts, locales)
│   └── etc/                System configuration
├── users/                  User home directories (encrypted)
│   └── default/            Default user
│       ├── desktop/        Desktop files
│       ├── documents/      Documents (encrypted)
│       ├── downloads/      Downloads (encrypted)
│       ├── .config/        App configs (encrypted)
│       ├── .cache/         App cache (cleared on reboot)
│       └── .local/         App data (encrypted)
├── persist/                Persistent data (if persistence enabled)
│   ├── packages/           Installed packages
│   └── snapshots/          System snapshots (for rollback)
├── tor/                    Tor data directory (on seL4 partition)
├── keys/                   Encryption keys (on seL4 partition)
└── tmp/                    Temporary files (RAM disk)

Filesystems:
  /          → read-only (squashfs or erofs)
  /users     → LUKS-encrypted ext4
  /persist   → LUKS-encrypted btrfs (for snapshots)
  /tmp       → tmpfs (RAM)
  /tor       → seL4-managed partition
  /keys      → seL4-managed partition
```

---

## 8. Network Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    APP LAYER                              │
│  Browser │ Chat │ Email │ AI │ Updates                    │
├─────────────────────────────────────────────────────────┤
│  WLT-Net (network manager)                               │
│  Routes ALL traffic to Tor gateway                       │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────┐    │
│  │  seL4: FIREWALL PARTITION                       │    │
│  │  Rule 1: DENY all non-Tor traffic              │    │
│  │  Rule 2: ALLOW only Tor gateway to use NIC     │    │
│  │  Rule 3: Kill switch — if Tor dies, block ALL  │    │
│  └─────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────┐    │
│  │  seL4: TOR GATEWAY PARTITION                    │    │
│  │  Tor daemon (3-hop onion routing)              │    │
│  │  DNS through Tor (no leaks)                    │    │
│  │  New circuit per app (compartmentalization)    │    │
│  └─────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────┤
│  NETWORK INTERFACE (WiFi/Ethernet)                       │
│  MAC address randomized on every boot                    │
└─────────────────────────────────────────────────────────┘
```

**No app can bypass Tor.** The firewall (in seL4) blocks all non-Tor traffic at the kernel level. Even if an app tries to make a direct connection, the firewall drops the packets.

---

## 9. Update System

### 9.1 A/B Partition Updates

```
Partition A: Current OS (active)
Partition B: Update target (inactive)

Update process:
1. Download update to Partition B (while OS runs on A)
2. Verify signature
3. Reboot into B
4. If boot succeeds → mark B as active
5. If boot fails → auto-rollback to A

Zero downtime, zero risk of bricking.
```

### 9.2 Package Manager (WLT-Pkg)

| Feature | Description |
|---------|-------------|
| **Atomic** | Install/update either fully succeeds or fully fails — no half-states |
| **Rollback** | Any package can be rolled back to any previous version |
| **Reproducible** | Same input = same output (verifiable builds) |
| **Signed** | All packages signed with developer key |
| **Delta updates** | Only download the changed parts |
| **Offline** | Can install from local cache |

---

## 10. AI Integration Architecture

```
┌─────────────────────────────────────────────────────────┐
│  WLT-AI (built into the OS, not an app)                  │
├─────────────────────────────────────────────────────────┤
│  Capabilities:                                           │
│  ├── Screen understanding (VLM — sees what you see)     │
│  ├── Voice commands (STT — always listening on hotword) │
│  ├── Voice responses (TTS — speaks back)                │
│  ├── System control (can open apps, change settings)    │
│  ├── File search ("find the PDF I downloaded last week")│
│  ├── Code assistance (built-in code editor integration) │
│  ├── Privacy analysis ("which apps are spying?")        │
│  └── Anonymity coach ("you're leaking DNS — fix it")    │
├─────────────────────────────────────────────────────────┤
│  Privacy:                                                │
│  ├── Runs LOCALLY (no cloud, unless user opts in)       │
│  ├── Can use cloud models if user enables               │
│  ├── Never sends data without explicit permission       │
│  └── AI memory is encrypted + amnesic option            │
├─────────────────────────────────────────────────────────┤
│  Models:                                                 │
│  ├── On-device: Llama-Vision, Phi-3 (for offline)       │
│  ├── Cloud (optional): Gemini, Claude, GLM (user picks) │
│  └── Routing: simple queries → local, complex → cloud   │
└─────────────────────────────────────────────────────────┘
```

---

## 11. Technology Stack Summary

| Component | Language | Why |
|-----------|---------|-----|
| Bootloader | Assembly + C | Low-level hardware control |
| seL4 partitions | C | seL4 is C-based |
| Linux kernel | C | (reused — already C) |
| GUI compositor | C | Performance-critical, wlroots is C |
| Widget toolkit | C | Performance, direct rendering |
| Package manager | Rust | Memory safety, modern |
| Security layer | Rust | Memory safety is critical |
| AI layer | Rust + Python | Rust for core, Python for ML |
| Core utilities | C | POSIX compatibility |
| Build system | Make + CMake | Standard for OS dev |

---

## 12. Hardware Requirements

### Minimum (target)

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| CPU | x86-64, 2 cores | x86-64, 4+ cores |
| RAM | 1GB | 4GB+ |
| Disk | 4GB | 20GB+ |
| GPU | Any (Mesa-supported) | NVIDIA/AMD/Intel |
| Network | WiFi or Ethernet | WiFi + Ethernet |
| Boot | UEFI | UEFI with Secure Boot |
| USB | 1 port (for Live USB) | 2+ ports |

### Supported Platforms

| Platform | Support | Notes |
|----------|---------|-------|
| x86-64 (Intel/AMD) | ✅ Primary | Full support |
| ARM64 (aarch64) | ⚠️ Future | seL4 supports it, Linux supports it |
| RISC-V | ⚠️ Future | seL4 has experimental support |

---

## 13. Development Environment

### 13.1 Tools Required

| Tool | Purpose | Version |
|------|---------|---------|
| **QEMU** | Virtual machine for testing | 8.0+ |
| **GCC cross-compiler** | Compile for x86-64 target | 13+ |
| **Rust** | For Rust components | 1.75+ |
| **NASM** | Assembler for bootloader | 2.16+ |
| **LD (linker)** | Link kernel + userspace | 2.40+ |
| **Make** | Build system | 4.4+ |
| **Git** | Version control | 2.40+ |
| **OVMF** | UEFI firmware for QEMU | Latest |
| **GCC + binutils** | For C compilation | 13+ |

### 13.2 Build Pipeline

```
1. Write code (C/Rust/Assembly)
2. Cross-compile for x86-64 target
3. Link into kernel image / userspace binaries
4. Create initramfs (cpio archive)
5. Build bootable .iso (with WLT-Boot + seL4 + Linux + initramfs)
6. Boot .iso in QEMU for testing
7. If stable → burn to USB for real hardware test
```

### 13.3 Testing Strategy

| Test Type | Tool | When |
|-----------|------|------|
| **Unit tests** | Rust cargo test, C check.h | Every commit |
| **Integration tests** | QEMU scripts | Every PR |
| **Boot tests** | QEMU + OVMF | Every build |
| **Hardware tests** | Real PC + USB | Every release |
| **Security audit** | Automated scanners | Every release |
| **Performance benchmarks** | Custom scripts | Every release |

---

## 14. Open Questions (To Resolve)

| # | Question | Options | Recommendation |
|---|----------|---------|----------------|
| 1 | **Display server** | Wayland (wlroots) vs custom | Wayland (wlroots) — don't reinvent |
| 2 | **libc** | musl vs glibc vs custom | musl — lighter, simpler |
| 3 | **Init system** | Custom vs OpenRC vs runit | Custom — smallest, fastest |
| 4 | **Package format** | Nix-style vs custom vs DEB/RPM | Nix-style — atomic, reproducible |
| 5 | **Browser** | Firefox-based vs WebKit-based vs custom | Firefox-based (like Tor Browser) |
| 6 | **Shell** | bash vs zsh vs custom | Custom (simpler, safer) |
| 7 | **AI model hosting** | On-device vs cloud vs hybrid | Hybrid (local for simple, cloud for complex) |
| 8 | **seL4 version** | Official vs Genode base | Official seL4 + custom partitions |

---

## 15. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| seL4 + Linux integration complexity | High | High | Start with seL4's reference VM example |
| GUI takes 3x longer than estimated | Very High | Medium | Start with wlroots, customize incrementally |
| Hardware compatibility issues | Medium | High | Linux kernel handles most hardware |
| Burnout (1-2 years is long) | High | Critical | AI assists with boilerplate; celebrate milestones |
| Scope creep | Very High | High | Strict MVP definition; freeze features per phase |
| seL4 learning curve | High | Medium | Study seL4 docs + tutorials first 2 weeks |
| No users (if that matters) | Medium | Low | Build for yourself first; community second |

---

## 16. Success Criteria

| Milestone | Criteria | Target |
|-----------|---------|--------|
| **MVP** | Boots in QEMU, shows desktop, has terminal | Month 6 |
| **Alpha** | Boots on real hardware, has browser + Tor | Month 12 |
| **Beta** | Daily-driver capable, has app ecosystem | Month 18 |
| **1.0** | Stable, documented, community-ready | Month 24 |

---

## Document Control

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-06-20 | WLTIOS Team | Initial architecture document |

---

*WLTIOS — Where Windows meets Linux meets Tails, inspired by all three.*
