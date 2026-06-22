# WLTIOS Module Map

> **Always current.** This is the map of every module in WLTIOS.
> If a module isn't here, it doesn't exist (officially).
> **Last updated:** 2026-06-22 (Area 4: wlroots)

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
│   └── 🔧 wlt-boot/ (C) — UEFI bootloader
│       ├── ✅ boot.c          # Main entry (efi_main) — IMPLEMENTED
│       ├── ✅ console.c       # UEFI console output, printf, colors — IMPLEMENTED
│       ├── ✅ memory.c        # UEFI memory map parsing — IMPLEMENTED
│       ├── ✅ loader.c        # seL4 ELF loader + UEFI file I/O — IMPLEMENTED
│       ├── ✅ verify.c        # SHA-256 (NIST verified) + sig check — IMPLEMENTED
│       ├── ✅ config.c        # Boot config INI parsing — IMPLEMENTED
│       ├── ✅ linker.ld       # UEFI linker script
│       └── ✅ Makefile        # Cross-compiler build rules
│
├── sel4-partitions/                              # seL4 isolated services
│   ├── 📋 firewall-pd/ (C) — Packet filtering + kill switch
│   ├── 📋 key-vault-pd/ (C) — Encryption key storage
│   └── 📋 boot-verifier-pd/ (C) — Runtime integrity check (spec needs correction)
│
├── kernel/                                        # Linux kernel (reused)
│   └── 💡 (Linux LTS 6.x — minimal config, not written by us)
│
├── userspace/                                     # Custom userspace
│   │
│   ├── 🔧 wlt-init/ (C) — Custom init system (PID 1)
│   │   ├── ✅ init.c          # Main entry — IMPLEMENTED (boot sequence)
│   │   ├── ✅ service.c       # Service loading + fork/exec — IMPLEMENTED
│   │   ├── ✅ monitor.c       # Process monitoring + crash recovery — IMPLEMENTED
│   │   ├── ✅ dependency.c    # Kahn topological sort + cycle detection — IMPLEMENTED
│   │   ├── ✅ tor.c           # Tor status + kill switch — IMPLEMENTED
│   │   ├── ✅ mount.c         # Filesystem mounting — IMPLEMENTED
│   │   ├── ✅ log.c           # Logging (file + auto-stderr) — IMPLEMENTED
│   │   ├── ✅ ipc.c           # Unix socket server for wltctl — IMPLEMENTED
│   │   ├── ✅ ctl.c           # wltctl CLI tool — IMPLEMENTED
│   │   ├── ✅ shutdown.c      # Clean shutdown + reboot — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 🔧 wlt-terminal/ (C) — Terminal emulator
│   │   ├── ✅ terminal.c      # Cell grid, cursor, ANSI dispatch — IMPLEMENTED
│   │   ├── ✅ pty.c           # openpty + fork + execvp — IMPLEMENTED
│   │   ├── ✅ ansi.c          # VT500 parser, CSI/SGR/OSC — IMPLEMENTED
│   │   ├── ✅ render.c        # BGRA framebuffer, 8x8 font — IMPLEMENTED
│   │   ├── ✅ scrollback.c    # Circular buffer 10000 lines — IMPLEMENTED
│   │   ├── ✅ config.c        # INI parser with color schemes — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 🔧 wlt-net/ (C) — Network manager (Tor-only)
│   │   ├── ✅ net.c           # Event loop, CLI dispatch, main() — IMPLEMENTED
│   │   ├── ✅ wifi.c          # wpa_supplicant control socket — IMPLEMENTED
│   │   ├── ✅ ethernet.c      # DHCP/static via iproute2 — IMPLEMENTED
│   │   ├── ✅ tor_proxy.c     # SOCKS5 probe, NEWNYM — IMPLEMENTED
│   │   ├── ✅ mac.c           # MAC randomization — IMPLEMENTED
│   │   ├── ✅ kill_switch.c   # nftables + iptables Tor-only — IMPLEMENTED
│   │   ├── ✅ config.c        # INI parser — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 🔧 wlt-crypto/ (C) — Disk + file encryption
│   │   ├── ✅ crypto.c        # Main dispatcher — IMPLEMENTED
│   │   ├── ⚠️ keyvault.c      # Unix socket IPC — NEEDS FIX (should be seL4 IPC stub)
│   │   ├── ✅ file_crypto.c   # Streaming chunk encrypt/decrypt — IMPLEMENTED
│   │   ├── ✅ secure_delete.c # 3-pass shred + unlink — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 🔧 wlt-sandbox/ (C) — App sandboxing
│   │   ├── ✅ sandbox.c       # cgroup v2 + namespaces + seccomp-BPF — IMPLEMENTED
│   │   ├── ✅ manifest.c      # INI parser for .manifest files — IMPLEMENTED
│   │   ├── ✅ launch.c        # Fork + sandbox setup + exec — IMPLEMENTED
│   │   ├── ✅ monitor.c       # OOM detection, seccomp violations — IMPLEMENTED
│   │   ├── ✅ audit.c         # Append-only security log — IMPLEMENTED
│   │   └── ✅ Makefile
│   │
│   ├── 📋 wlt-compositor/ (C + wlroots) — Wayland compositor
│   ├── 💡 wlt-render/ (C + OpenGL/Vulkan) — Rendering engine
│   ├── 💡 wlt-widgets/ (C) — Custom widget toolkit
│   ├── 📋 wlt-shell/ (C) — Desktop shell (taskbar, launcher)
│   ├── 📋 wlt-pkg/ (Rust) — Package manager (Nix-style)
│   ├── 📋 wlt-tor-mgr/ (Rust) — Tor control panel
│   ├── 📋 wlt-permissions/ (Rust) — App permission system
│   ├── 📋 wlt-ai/ (Rust + Python) — AI assistant
│   ├── 📋 wlt-settings/ (C) — Settings app
│   ├── 📋 wlt-filemanager/ (C) — File manager
│   ├── 📋 wlt-update/ (Rust) — Atomic A/B update system
│   └── 📋 (3 seL4 partition specs need correction — see Claude's analysis)
│
├── tests/                                        # All tests
│   ├── ✅ boot/               # SHA-256 (4), Config (6), ELF (8) — 18 PASS
│   ├── ✅ userspace/          # Service (9), Dependency (5), Monitor (9) — 23 PASS
│   ├── 💡 sel4/               # seL4 partition tests
│   ├── 💡 kernel/             # Kernel module tests
│   ├── 💡 security/           # Security + penetration tests
│   └── 💡 performance/        # Performance benchmarks
│
├── config/                                       # OS config files
│   ├── ✅ services/           # 8 .service files (key-vault, tor-vm, wlt-net, etc.)
│   ├── ✅ wlt-compositor.conf # Wayland compositor config
│   ├── ✅ wlt-net.conf        # Network manager config
│   └── ✅ wlt-tor.conf        # Tor manager config
│
├── scripts/                                      # Automation
│   ├── ✅ auto-commit.sh     # Auto-commit + push to GitHub
│   ├── ✅ build.sh           # Full build (seL4 + WLT-Boot + WLT-Init)
│   ├── ✅ test.sh            # Run all tests
│   ├── ✅ qemu-test.sh       # Boot test in QEMU
│   └── ✅ pre-commit-check.sh # Security + lint checks
│
└── docs/                                         # Documentation
    ├── ✅ ARCHITECTURE.md     # Technical blueprint (619 lines)
    ├── ✅ PRD.md              # Product requirements (501 lines)
    ├── ✅ ROADMAP.md          # 24-month plan (505 lines)
    ├── ✅ ENGINEERING_FRAMEWORK.md # 10 rules (constitution)
    ├── ✅ AGENT_ORCHESTRATION.md  # 7 agent roles
    ├── ✅ ONBOARDING.md       # 1-hour guide
    ├── ✅ CODING_STYLE.md     # Enforced rules
    ├── ✅ MODULE_MAP.md       # This file
    ├── ✅ CONTRIBUTING.md
    ├── ✅ specs/              # 19 module specs
    ├── ✅ adr/                # 7 ADRs
    └── ✅ context/            # Research + recovery files
```

---

## Status Summary

| Layer | Modules | Code Written | Tests | % Complete |
|-------|---------|-------------|-------|------------|
| Boot | 1 (wlt-boot) | ✅ 1,500 lines | 18 pass | ~40% |
| seL4 Partitions | 3 | 0 | 0 | 0% (specs need correction) |
| Kernel | 1 (reused) | 0 | 0 | N/A |
| Userspace | 14 total | 6 modules (~13,000 lines) | 23 pass | ~35% |
| Tests | 5 dirs | ~1,000 lines | 41/41 pass | ✅ |
| Config | 8 service + 3 conf | ~300 lines | — | ✅ Phase 1 |
| Scripts | 5 | ~500 lines | — | ✅ |
| Docs | 30+ files | ~16,000 lines | — | ✅ Phase 1 |
| **Total** | **~24 modules** | **~15,300 lines** | **41 pass** | **~20%** |

## Spec Status (19 specs — all complete)

All 18 original specs + 1 notes file. Some specs need corrections per Claude's analysis:
- ⚠️ `key-vault-pd.md` — persistence design gap (keys volatile, need disk storage)
- ⚠️ `boot-verifier-pd.md` — can't access filesystem from seL4 PD (needs correction)
- ⚠️ `wlt-net.md` — spec says Rust, code is C (language mismatch)
- ⚠️ `wlt-sandbox.md` — spec says Rust, code is C (language mismatch)

## Test Status

```
make -f tests/Makefile test
→ 41/41 tests PASS (SHA-256, Config, ELF, Service, Dependency, Monitor)
```

## Critical Path (Phase 1)

```
wlt-boot → seL4 boots → Linux boots → wlt-init → desktop
     ✅         ✅           ✅          🔧         ❌
```

## Known Issues (from Claude's audit 2026-06-22)

1. ⚠️ keyvault.c uses Unix sockets — architecturally wrong (seL4 PDs can't use sockets)
2. ⚠️ wlt-init uses malloc — violates CODING_STYLE.md (PID 1 should use static buffers)
3. ⚠️ ADR-04 says Microkit but actual build uses CAmkES — needs note
4. ⚠️ Boot Verifier PD spec claims filesystem access — impossible from seL4 PD
5. ⚠️ Key Vault PD spec doesn't resolve key persistence across reboots
6. ⚠️ wlt-net, wlt-sandbox written in C but specs say Rust
7. ⚠️ New modules (wlt-crypto, wlt-net, wlt-sandbox, wlt-terminal) have no tests yet

---

**This map is updated whenever a module is added, removed, or changes status.**
