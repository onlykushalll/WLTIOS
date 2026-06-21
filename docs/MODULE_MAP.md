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
│   └── 📋 wlt-boot/ (C) — UEFI bootloader
│       ├── ✅ boot.c          # Main entry (efi_main) — SKELETON
│       ├── ✅ console.c       # UEFI console output — SKELETON
│       ├── ✅ memory.c        # UEFI memory map parsing — SKELETON
│       ├── ✅ loader.c        # seL4 ELF loader — SKELETON
│       ├── ✅ verify.c        # Signature verification (SHA-256) — SKELETON
│       ├── ✅ config.c        # Boot config parsing — SKELETON
│       └── ✅ Makefile        # Cross-compiler build rules
│
├── sel4-partitions/                              # seL4 isolated services
│   ├── 📋 firewall-pd/ (C) — Packet filtering + kill switch
│   ├── 📋 key-vault-pd/ (C) — Encryption key storage
│   └── 📋 boot-verifier-pd/ (C) — Runtime integrity check
│
├── kernel/                                        # Linux kernel (reused)
│   └── 💡 (Linux LTS 6.x — minimal config, not written by us)
│
├── userspace/                                     # Custom userspace
│   │
│   ├── 📋 wlt-init/ (C) — Custom init system (PID 1)
│   │   ├── ✅ init.c          # Main entry — SKELETON (boot sequence)
│   │   ├── ✅ service.c       # Service loading + parsing — SKELETON
│   │   ├── ✅ monitor.c       # Process monitoring — SKELETON
│   │   ├── ✅ dependency.c    # Dependency graph — SKELETON
│   │   ├── ✅ tor.c           # Tor status + kill switch — SKELETON
│   │   ├── ✅ mount.c         # Filesystem mounting — SKELETON
│   │   ├── ✅ log.c           # Logging — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 📋 wlt-compositor/ (C + wlroots) — Wayland compositor
│   ├── 💡 wlt-render/ (C + OpenGL/Vulkan) — Rendering engine
│   ├── 💡 wlt-widgets/ (C) — Custom widget toolkit
│   ├── 📋 wlt-shell/ (C) — Desktop shell (taskbar, launcher)
│   ├── 📋 wlt-pkg/ (Rust) — Package manager (Nix-style)
│   ├── 📋 wlt-net/ (Rust) — Network manager (Tor-only)
│   ├── 📋 wlt-sandbox/ (Rust) — App sandboxing
│   ├── 📋 wlt-crypto/ (C + libsodium) — Disk + file encryption
│   ├── 📋 wlt-tor-mgr/ (Rust) — Tor control panel
│   ├── 📋 wlt-permissions/ (Rust) — App permission system
│   ├── 📋 wlt-ai/ (Rust + Python) — AI assistant
│   ├── 📋 wlt-settings/ (C) — Settings app
│   ├── 📋 wlt-filemanager/ (C) — File manager
│   ├── 📋 wlt-terminal/ (C) — Terminal emulator
│   └── 📋 wlt-update/ (Rust) — Atomic A/B update system
│
├── tools/                                        # Build tools
│   └── 💡 (build.sh, test.sh, etc. — in scripts/)
│
├── scripts/                                      # Automation
│   ├── ✅ auto-commit.sh     # Auto-commit + push to GitHub
│   ├── ✅ build.sh           # Full build (seL4 + WLT-Boot + WLT-Init)
│   ├── ✅ test.sh            # Run all tests
│   ├── ✅ qemu-test.sh       # Boot test in QEMU
│   └── ✅ pre-commit-check.sh # Security + lint checks
│
├── tests/                                        # All tests
│   ├── 💡 boot/               # Bootloader tests
│   ├── 💡 sel4/               # seL4 partition tests
│   ├── 💡 kernel/             # Kernel module tests
│   ├── 💡 userspace/          # Userspace tests
│   ├── 💡 security/           # Security + penetration tests
│   └── 💡 performance/        # Performance benchmarks
│
└── docs/                                         # Documentation
    ├── ✅ ARCHITECTURE.md
    ├── ✅ PRD.md
    ├── ✅ ROADMAP.md
    ├── ✅ ENGINEERING_FRAMEWORK.md
    ├── ✅ AGENT_ORCHESTRATION.md
    ├── ✅ ONBOARDING.md
    ├── ✅ CODING_STYLE.md
    ├── ✅ MODULE_MAP.md (this file)
    ├── ✅ CONTRIBUTING.md
    ├── ✅ specs/              # 18 module specs
    ├── ✅ adr/                # 7 Architecture Decision Records
    └── ✅ context/            # Research + recovery files
```

---

## Status Summary

| Layer | Modules Planned | Spec'd | Skeleton | Implemented | % Complete |
|-------|-----------------|--------|----------|-------------|------------|
| Boot | 1 (wlt-boot) | 1 ✅ | 1 ✅ | 0 | 30% (spec + skeleton) |
| seL4 Partitions | 3 | 3 ✅ | 0 | 0 | 15% (spec only) |
| Kernel | 1 (reused) | 0 | 0 | 0 | 0% (not started) |
| Userspace | 14 | 14 ✅ | 1 ✅ | 0 | 15% (specs + 1 skeleton) |
| Tools/Scripts | 5 | — | 5 ✅ | 5 ✅ | 100% |
| Tests | 5 dirs | — | 0 | 0 | 0% (dirs created) |
| Docs | 10+ | — | — | 10+ ✅ | 100% (Phase 1 docs) |
| **Total** | **~24 modules** | **18** | **2** | **1 (log.c)** | **~20%** |

## Spec Status (18/18 complete)

| Module | Spec File | Status |
|--------|-----------|--------|
| WLT-Boot | specs/wlt-boot.md | ✅ |
| WLT-Init | specs/wlt-init.md | ✅ |
| WLT-Compositor | specs/wlt-compositor.md | ✅ |
| WLT-Shell | specs/wlt-shell.md | ✅ |
| WLT-Pkg | specs/wlt-pkg.md | ✅ |
| WLT-Net | specs/wlt-net.md | ✅ |
| WLT-Sandbox | specs/wlt-sandbox.md | ✅ |
| WLT-Crypto | specs/wlt-crypto.md | ✅ |
| WLT-Tor-Mgr | specs/wlt-tor-mgr.md | ✅ |
| WLT-Permissions | specs/wlt-permissions.md | ✅ |
| WLT-AI | specs/wlt-ai.md | ✅ |
| WLT-Settings | specs/wlt-settings.md | ✅ |
| WLT-FileManager | specs/wlt-filemanager.md | ✅ |
| WLT-Terminal | specs/wlt-terminal.md | ✅ |
| WLT-Update | specs/wlt-update.md | ✅ |
| Firewall PD | specs/firewall-pd.md | ✅ |
| Key Vault PD | specs/key-vault-pd.md | ✅ |
| Boot Verifier PD | specs/boot-verifier-pd.md | ✅ |

## Critical Path (Phase 1)

```
wlt-boot → seL4 boots → Linux boots → wlt-init → wlt-compositor → desktop
```

This is the minimum viable path. Everything else is built on top.

---

**This map is updated whenever a module is added, removed, or changes status.**
