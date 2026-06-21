# WLTIOS Module Map

> **Always current.** This is the map of every module in WLTIOS.
> If a module isn't here, it doesn't exist (officially).
> **Last updated:** 2026-06-21

## Legend

```
[STATUS] Module Name (Language) — Purpose
  STATUS: ✅ Implemented | 🔧 In Progress | 📋 Spec'd | 💡 Planned | 🔬 Researching
```

---

## Module Tree

```
WLTIOS/
│
├── boot/                                          # Boot system
│   └── wlt-boot/ (C) — UEFI bootloader
│       ├── boot.c          # Main entry (efi_main)
│       ├── memory.c        # UEFI memory map parsing
│       ├── loader.c        # seL4 ELF loader
│       ├── verify.c        # Signature verification (SHA-256)
│       ├── config.c        # Boot config parsing
│       └── console.c       # UEFI console output
│
├── sel4-partitions/                              # seL4 isolated services
│   ├── firewall-pd/ (C) — Packet filtering + kill switch
│   │   └── (Planned — see RESEARCH_SEL4_PARTITIONS.md)
│   ├── key-vault-pd/ (C) — Encryption key storage
│   │   └── (Planned)
│   ├── boot-verifier-pd/ (C) — Runtime integrity check
│   │   └── (Planned)
│   └── (Tor runs in second Linux VM — see ADR-03)
│
├── kernel/                                        # Linux kernel (reused)
│   └── (Linux LTS 6.x — minimal config, not written by us)
│
├── userspace/                                     # Custom userspace
│   │
│   ├── wlt-init/ (C) — Custom init system (PID 1)
│   │   ├── init.c          # Main entry
│   │   ├── service.c       # Service loading + parsing
│   │   ├── monitor.c       # Process monitoring
│   │   ├── dependency.c    # Dependency graph
│   │   ├── tor.c           # Tor status tracking
│   │   ├── mount.c         # Filesystem mounting
│   │   ├── shutdown.c      # Shutdown sequence
│   │   ├── log.c           # Logging
│   │   └── ctl.c           # wltctl command handler
│   │
│   ├── wlt-compositor/ (C + wlroots) — Wayland compositor
│   │   └── (Planned — see ADR-05, RESEARCH_TAILS_WLROOTS.md)
│   │
│   ├── wlt-render/ (C + OpenGL/Vulkan) — Rendering engine
│   │   └── (Planned — shadows, blur, animations)
│   │
│   ├── wlt-widgets/ (C) — Custom widget toolkit
│   │   └── (Planned — buttons, menus, dialogs)
│   │
│   ├── wlt-shell/ (C + wlt-widgets) — Desktop shell
│   │   └── (Planned — taskbar, launcher, notifications)
│   │
│   ├── wlt-pkg/ (Rust) — Package manager
│   │   └── (Planned — Nix-style, atomic, rollback)
│   │
│   ├── wlt-net/ (Rust) — Network manager
│   │   └── (Planned — WiFi, Ethernet, VPN config)
│   │
│   ├── wlt-sandbox/ (Rust) — App sandboxing
│   │   └── (Planned — cgroups + namespaces wrapper)
│   │
│   ├── wlt-crypto/ (C + libsodium) — Disk + file encryption
│   │   └── (Planned — LUKS integration)
│   │
│   ├── wlt-tor-mgr/ (Rust) — Tor control panel
│   │   └── (Planned — circuit switching, identity)
│   │
│   ├── wlt-permissions/ (Rust) — App permission system
│   │   └── (Planned — network, files, camera)
│   │
│   ├── wlt-ai/ (Rust + Python) — AI assistant
│   │   ├── core/           # LLM integration, tool use
│   │   ├── ui/             # Sidebar, voice, commands
│   │   ├── vision/         # Screen understanding (VLM)
│   │   └── audio/          # Voice I/O (STT, TTS)
│   │   └── (Planned)
│   │
│   ├── wlt-settings/ (C + wlt-widgets) — Settings app
│   │   └── (Planned)
│   │
│   ├── wlt-filemanager/ (C + wlt-widgets) — File manager
│   │   └── (Planned — with Tor download + encryption)
│   │
│   ├── wlt-terminal/ (C + wlt-widgets) — Terminal emulator
│   │   └── (Planned)
│   │
│   └── wlt-update/ (Rust) — Atomic update system
│       └── (Planned — A/B partition swap)
│
├── tools/                                        # Build tools
│   ├── build.sh            # Build the OS
│   ├── test.sh             # Run all tests
│   ├── qemu-test.sh        # Boot test in QEMU
│   └── pre-commit-check.sh # Security + lint checks
│
├── scripts/                                      # Automation
│   └── auto-commit.sh     # Auto-commit + push to GitHub
│
├── tests/                                        # All tests
│   ├── boot/               # Bootloader tests
│   ├── sel4/               # seL4 partition tests
│   ├── kernel/             # Kernel module tests
│   ├── userspace/          # Userspace tests
│   ├── security/           # Security + penetration tests
│   └── performance/        # Performance benchmarks
│
└── docs/                                         # Documentation
    ├── ARCHITECTURE.md
    ├── PRD.md
    ├── ROADMAP.md
    ├── ENGINEERING_FRAMEWORK.md
    ├── AGENT_ORCHESTRATION.md
    ├── ONBOARDING.md
    ├── CODING_STYLE.md
    ├── MODULE_MAP.md (this file)
    ├── specs/              # Module specs (before code)
    ├── adr/                # Architecture Decision Records
    └── context/            # Research + recovery files
```

---

## Status Summary

| Layer | Modules Planned | Implemented | % Complete |
|-------|-----------------|-------------|------------|
| Boot | 1 (wlt-boot) | 0 | 0% |
| seL4 Partitions | 3 (firewall, key-vault, boot-verifier) | 0 | 0% |
| Kernel | 1 (Linux, reused) | 0 (not configured) | 0% |
| Userspace | 14 modules | 0 | 0% |
| Tools/Scripts | 5 | 1 (auto-commit.sh) | 20% |
| Docs | 10+ | 10+ | 100% (Phase 1 docs) |
| **Total** | **~24 modules** | **1** | **~4%** |

## Dependencies Graph (Simplified)

```
wlt-boot → seL4 → Linux guest
                   ↓
               wlt-init → wlt-crypto → key-vault-pd (seL4)
                   ↓
               wlt-tor-mgr → tor-vm (second Linux) → firewall-pd (seL4)
                   ↓
               wlt-net → wlt-compositor → wlt-shell → wlt-ai
                                              ↓
                                         wlt-widgets → wlt-settings
                                                      → wlt-filemanager
                                                      → wlt-terminal
```

## Critical Path (Phase 1)

```
wlt-boot → seL4 boots → Linux boots → wlt-init → wlt-compositor → desktop
```

This is the minimum viable path. Everything else is built on top.

---

**This map is updated whenever a module is added, removed, or changes status.**
