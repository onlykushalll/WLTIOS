# WLTIOS — Development Roadmap

> **Timeline:** 24 months (2 years)
> **Start Date:** 2026-06-20
> **Target 1.0:** 2028-06-20

---

## Overview

```
Phase 1: Foundation     Months 1-6    →  MVP (boots in QEMU)
Phase 2: Desktop        Months 7-12   →  Alpha (usable desktop)
Phase 3: Security       Months 13-18  →  Beta (full anonymity)
Phase 4: Polish + AI    Months 19-24  →  1.0 (production ready)
```

---

## Phase 1: Foundation (Months 1-6)

### Goal
Prove the architecture works. Bootable OS in QEMU.

### Month 1: Setup + Bootloader

**Week 1-2: Development Environment**
- [ ] Install QEMU + OVMF (UEFI firmware) on dev PC
- [ ] Install cross-compiler (gcc-x86-64, nasm, ld)
- [ ] Install Rust + cargo
- [ ] Set up Git repository structure
- [ ] Create build system (Makefile)
- [ ] Test: "Hello World" bootloader boots in QEMU

**Week 3-4: WLT-Boot (Custom Bootloader)**
- [ ] Write UEFI bootloader in Assembly + C
- [ ] Implement multiboot2 protocol
- [ ] Load seL4 into memory
- [ ] Transfer control to seL4
- [ ] Test: WLT-Boot → seL4 boots in QEMU

**Deliverable:** `wlt-boot.bin` — boots seL4 in QEMU

### Month 2: seL4 + Linux Integration

**Week 5-6: seL4 Setup**
- [ ] Study seL4 documentation + tutorials
- [ ] Build seL4 for x86-64
- [ ] Create seL4 partition for Linux VM
- [ ] Configure seL4 to load Linux as guest
- [ ] Test: seL4 boots, Linux starts as guest

**Week 7-8: Linux Guest Configuration**
- [ ] Configure minimal Linux kernel (only essential drivers)
- [ ] Create initramfs (initial RAM filesystem)
- [ ] Set up Linux → seL4 communication channel
- [ ] Test: Linux boots as guest, can print to screen

**Deliverable:** `wltios.iso` — seL4 + Linux boots in QEMU

### Month 3: Init System + Shell

**Week 9-10: WLT-Init (Custom Init)**
- [ ] Write init system in C (replaces systemd)
- [ ] Implement service startup
- [ ] Implement dependency resolution
- [ ] Test: init starts, launches shell

**Week 11-12: Minimal Shell**
- [ ] Write basic shell (command interpreter)
- [ ] Implement core commands: ls, cd, cat, echo, mkdir, rm
- [ ] Implement path resolution
- [ ] Test: can navigate filesystem, run commands

**Deliverable:** Bootable OS with working shell

### Month 4: Terminal + File Manager

**Week 13-14: Terminal Emulator**
- [ ] Write terminal emulator (C + framebuffer)
- [ ] Implement VT100 escape sequences
- [ ] Connect to shell via PTY
- [ ] Test: terminal displays shell output, accepts input

**Week 15-16: Basic File Manager**
- [ ] Write file manager (C)
- [ ] List directories
- [ ] Navigate folders
- [ ] Copy/move/delete files
- [ ] Test: can manage files via GUI

**Deliverable:** OS with terminal + file manager

### Month 5: Basic GUI

**Week 17-18: Framebuffer + Input**
- [ ] Set up framebuffer display
- [ ] Handle keyboard input
- [ ] Handle mouse input
- [ ] Test: can draw pixels, read input

**Week 19-20: Window System (Basic)**
- [ ] Write basic window manager
- [ ] Create windows (move, resize, close)
- [ ] Render text in windows
- [ ] Test: multiple windows work

**Deliverable:** OS with basic GUI windows

### Month 6: Tor + MVP

**Week 21-22: Network + Tor**
- [ ] Configure Linux network stack
- [ ] Install Tor daemon
- [ ] Route traffic through Tor
- [ ] Test: can access .onion sites

**Week 23-24: MVP Polish**
- [ ] Fix critical bugs
- [ ] Optimize boot time
- [ ] Document build process
- [ ] Create MVP release

**MVP Deliverable:**
- ✅ Boots in QEMU in < 5 seconds
- ✅ Custom bootloader (WLT-Boot)
- ✅ seL4 + Linux integration
- ✅ Terminal emulator
- ✅ File manager
- ✅ Basic GUI (windows)
- ✅ Tor connectivity
- ✅ Idle RAM < 512MB

---

## Phase 2: Desktop (Months 7-12)

### Goal
Usable desktop environment. Daily-driver potential.

### Month 7-8: Wayland Compositor

**Week 25-28: WLT-Compositor**
- [ ] Study wlroots library
- [ ] Implement Wayland compositor on wlroots
- [ ] Handle outputs (displays)
- [ ] Handle inputs (keyboard, mouse, touch)
- [ ] Render surfaces (windows)
- [ ] Test: Wayland apps can display

**Week 29-30: Window Management**
- [ ] Move windows (drag)
- [ ] Resize windows (drag edges)
- [ ] Close windows
- [ ] Minimize/maximize
- [ ] Window stacking (z-order)
- [ ] Test: full window management works

**Week 31-32: Window Snapping**
- [ ] Snap to half screen (left/right)
- [ ] Snap to quarter screen (corners)
- [ ] Snap to full screen
- [ ] Alt+Tab app switcher
- [ ] Test: snapping works smoothly

### Month 9: Taskbar + Launcher

**Week 33-34: WLT-Shell (Taskbar)**
- [ ] Design taskbar (position, style)
- [ ] Show running apps
- [ ] App switching (click to focus)
- [ ] System tray (network, battery, clock, Tor)
- [ ] Test: taskbar functional

**Week 35-36: App Launcher**
- [ ] Design launcher (search + launch)
- [ ] Index installed apps
- [ ] Search by name
- [ ] Launch on Enter
- [ ] Test: launcher works

### Month 10: Settings + Package Manager

**Week 37-38: WLT-Settings**
- [ ] Settings app structure
- [ ] Network settings (WiFi, Tor config)
- [ ] Display settings (resolution, brightness)
- [ ] Sound settings (volume)
- [ ] User settings (account, password)
- [ ] Test: settings persist

**Week 39-40: WLT-Pkg (Package Manager)**
- [ ] Design package format (Nix-style)
- [ ] Install packages
- [ ] Remove packages
- [ ] List installed packages
- [ ] Update packages
- [ ] Test: package operations work

### Month 11: Web Browser + Disk Encryption

**Week 41-42: Tor Browser Integration**
- [ ] Build Tor Browser (Firefox fork)
- [ ] Configure for maximum privacy
- [ ] Integrate with system Tor
- [ ] Test: can browse web anonymously

**Week 43-44: Disk Encryption (LUKS)**
- [ ] Implement LUKS encryption
- [ ] Encrypt user home directory
- [ ] Encrypt persistent storage
- [ ] Test: data is encrypted at rest

### Month 12: App Sandboxing + Alpha Release

**Week 45-46: App Sandboxing**
- [ ] Implement cgroups wrapper
- [ ] Implement namespaces wrapper
- [ ] Create app manifest format
- [ ] Enforce permissions
- [ ] Test: apps are isolated

**Week 47-48: Alpha Polish + Release**
- [ ] Bug fixing
- [ ] Performance optimization
- [ ] Documentation
- [ ] Alpha release

**Alpha Deliverable:**
- ✅ Custom Wayland compositor
- ✅ Full window management
- ✅ Taskbar + app launcher
- ✅ Settings app
- ✅ Package manager
- ✅ Web browser (Tor Browser)
- ✅ Disk encryption
- ✅ App sandboxing (basic)
- ✅ Boots on real hardware
- ✅ Idle RAM < 384MB

---

## Phase 3: Security + Anonymity (Months 13-18)

### Goal
Full security and anonymity features. Production-ready.

### Month 13-14: seL4 Partitions

**Week 49-52: Tor Partition**
- [ ] Create seL4 partition for Tor daemon
- [ ] Move Tor from Linux to seL4
- [ ] Set up IPC between Linux and Tor partition
- [ ] Test: Tor runs isolated from Linux

**Week 53-54: Key Vault Partition**
- [ ] Create seL4 partition for key storage
- [ ] Move encryption keys to seL4
- [ ] Implement encrypt/decrypt IPC
- [ ] Test: keys are isolated

### Month 15: Firewall + Boot Verifier

**Week 55-56: Firewall Partition**
- [ ] Create seL4 partition for firewall
- [ ] Implement packet filtering
- [ ] Enforce "Tor-only" rule
- [ ] Implement kill switch
- [ ] Test: non-Tor traffic is blocked

**Week 57-58: Boot Verifier Partition**
- [ ] Create seL4 partition for boot verification
- [ ] Check kernel signatures at runtime
- [ ] Detect rootkits
- [ ] Test: tampering is detected

### Month 16: Anonymity Features

**Week 59-60: MAC Randomization + Kill Switch**
- [ ] Randomize MAC on every boot
- [ ] Implement kill switch (Tor dies → network dies)
- [ ] DNS through Tor (no leaks)
- [ ] Test: no DNS leaks, kill switch works

**Week 61-62: Amnesic Mode**
- [ ] Implement Live USB boot
- [ ] tmpfs for all writable paths
- [ ] Nothing touches disk unless saved
- [ ] Test: no trace after reboot

### Month 17: Permissions + Privacy

**Week 63-64: App Permission System**
- [ ] Design permission manifest
- [ ] Network: tor-only / direct / none
- [ ] Files: sandbox / home / full
- [ ] Camera/Mic: allow / deny / ask
- [ ] Test: permissions enforced

**Week 65-66: Privacy Badges + Secure Delete**
- [ ] Privacy badge per app (🟢🟡🔴)
- [ ] Secure delete (shred files)
- [ ] Tor circuit viewer
- [ ] Test: badges accurate, delete is secure

### Month 18: Beta Release

**Week 67-68: Beta Polish**
- [ ] Security audit
- [ ] Penetration testing
- [ ] Bug fixing
- [ ] Performance optimization
- [ ] Documentation
- [ ] Beta release

**Beta Deliverable:**
- ✅ seL4 partitions (Tor, keys, firewall, boot verifier)
- ✅ Kill switch (< 100ms trigger)
- ✅ MAC randomization
- ✅ Amnesic mode (Live USB)
- ✅ App permission system
- ✅ Privacy badges
- ✅ Secure delete
- ✅ Tor circuit viewer
- ✅ Idle RAM < 256MB
- ✅ Boot < 3 seconds

---

## Phase 4: Polish + AI (Months 19-24)

### Goal
Production-ready, polished, AI-integrated.

### Month 19-20: AI Assistant

**Week 73-76: WLT-AI Core**
- [ ] AI assistant engine (Rust)
- [ ] LLM integration (local: Llama-Vision)
- [ ] Tool use (can call OS functions)
- [ ] Screen understanding (VLM)
- [ ] Test: AI can see screen, answer questions

**Week 77-78: AI UI**
- [ ] AI sidebar (always available)
- [ ] Voice input (STT)
- [ ] Voice output (TTS)
- [ ] System control (open apps, change settings)
- [ ] Test: AI can control OS

**Week 79-80: AI Features**
- [ ] File search ("find my PDF from last week")
- [ ] Privacy analysis ("which apps are spying?")
- [ ] Anonymity coach ("you're leaking DNS")
- [ ] Optional cloud models (Gemini, Claude)
- [ ] Test: all AI features work

### Month 21: Update System

**Week 81-82: A/B Partition Updates**
- [ ] Implement A/B partition scheme
- [ ] Download update to inactive partition
- [ ] Verify signature
- [ ] Reboot into new partition
- [ ] Auto-rollback if boot fails
- [ ] Test: updates + rollback work

**Week 83-84: Package Repository**
- [ ] Set up package repository (hosted)
- [ ] Package signing (GPG)
- [ ] Delta updates (only download changes)
- [ ] Offline install from cache
- [ ] Test: can install from repo

### Month 22: Polish

**Week 85-86: UI Polish**
- [ ] Animations (window open/close, transitions)
- [ ] Themes (dark/light, custom)
- [ ] Icons (consistent set)
- [ ] Fonts (readable, multilingual)
- [ ] Test: UI feels polished

**Week 87-88: Hardware Compatibility**
- [ ] Test on multiple PCs
- [ ] Fix hardware-specific bugs
- [ ] Multi-monitor support
- [ ] Bluetooth support
- [ ] Touchpad gestures
- [ ] Test: works on common hardware

### Month 23: Documentation + Community

**Week 89-90: Documentation**
- [ ] User guide (installation, usage)
- [ ] Developer guide (architecture, contributing)
- [ ] Security whitepaper
- [ ] API documentation
- [ ] Test: docs are complete

**Week 91-92: Community Setup**
- [ ] Website
- [ ] Forum/Matrix channel
- [ ] Bug tracker
- [ ] Contribution guidelines
- [ ] Test: community can participate

### Month 24: 1.0 Release

**Week 93-94: Final Testing**
- [ ] 30-day daily-driver test
- [ ] Security audit (final)
- [ ] Performance benchmarks
- [ ] Bug fixing
- [ ] Release candidate

**Week 95-96: 1.0 Release**
- [ ] Final polish
- [ ] Release announcement
- [ ] Distribution (ISO download)
- [ ] Documentation published
- [ ] Community launch

**1.0 Deliverable:**
- ✅ AI assistant (local + optional cloud)
- ✅ Atomic updates with rollback
- ✅ Polished UI (animations, themes)
- ✅ Multi-monitor + Bluetooth
- ✅ Full documentation
- ✅ Community platform
- ✅ Idle RAM < 256MB
- ✅ Boot < 3 seconds
- ✅ Daily-driver capable

---

## Milestone Summary

| Milestone | Month | Key Achievement |
|-----------|-------|-----------------|
| **M1: Hello World** | Month 1 | Bootloader boots in QEMU |
| **M2: seL4 + Linux** | Month 2 | seL4 boots Linux as guest |
| **M3: Shell Works** | Month 3 | Can run commands |
| **M4: Terminal + Files** | Month 4 | Terminal + file manager |
| **M5: Basic GUI** | Month 5 | Windows display |
| **M6: MVP** | Month 6 | Boots, has terminal, files, Tor |
| **M7: Compositor** | Month 7-8 | Wayland compositor works |
| **M8: Desktop** | Month 9 | Taskbar + launcher |
| **M9: Packages** | Month 10 | Package manager |
| **M10: Browser + Crypto** | Month 11 | Tor Browser + disk encryption |
| **M11: Sandboxing** | Month 12 | Apps sandboxed |
| **M12: ALPHA** | Month 12 | Usable desktop OS |
| **M13: seL4 Partitions** | Month 13-14 | Tor + keys in seL4 |
| **M14: Firewall + Verifier** | Month 15 | seL4 firewall + boot verifier |
| **M15: Anonymity** | Month 16 | Kill switch + amnesic mode |
| **M16: Permissions** | Month 17 | App permission system |
| **M17: BETA** | Month 18 | Full security + anonymity |
| **M18: AI Assistant** | Month 19-20 | Built-in AI |
| **M19: Updates** | Month 21 | Atomic A/B updates |
| **M20: Polish** | Month 22 | UI polished, hardware tested |
| **M21: Docs + Community** | Month 23 | Documentation + community |
| **M22: 1.0 RELEASE** | Month 24 | Production ready |

---

## Resource Estimates

### Development Hours Per Phase

| Phase | Duration | Est. Hours/Week | Total Hours |
|-------|----------|-----------------|-------------|
| Phase 1 (MVP) | 6 months | 40-60 | ~1,200 |
| Phase 2 (Alpha) | 6 months | 40-60 | ~1,200 |
| Phase 3 (Beta) | 6 months | 40-60 | ~1,200 |
| Phase 4 (1.0) | 6 months | 40-60 | ~1,200 |
| **TOTAL** | **24 months** | **40-60** | **~4,800** |

### Lines of Code Per Phase

| Phase | Lines Written | Cumulative |
|-------|--------------|------------|
| Phase 1 (MVP) | ~150K | 150K |
| Phase 2 (Alpha) | ~1M | 1.15M |
| Phase 3 (Beta) | ~800K | 1.95M |
| Phase 4 (1.0) | ~1M | 2.95M |
| **TOTAL** | **~3M** | **3M** |

---

## Risk Mitigation Timeline

| Risk | When It's Addressed | Mitigation |
|------|---------------------|-----------|
| seL4 learning curve | Month 2 | 2 weeks dedicated study |
| GUI complexity | Month 7-8 | Start with wlroots, increment |
| Hardware support | Month 12+ | Linux kernel handles most |
| Burnout | Ongoing | AI assists, celebrate milestones |
| Scope creep | Ongoing | Strict phase freeze |

---

*This roadmap is a living document. We will adjust as we learn.*

*WLTIOS — 24 months. 3 million lines. One vision.*
