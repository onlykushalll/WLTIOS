# WLTIOS

**Windows + Linux + Tails + Inspired OS**

A privacy-first desktop operating system built on seL4 microkernel isolation, combining the usability of Windows, the efficiency of Linux, and the anonymity of Tails — with a built-in AI guardian named Kairos.

```
seL4 (formally verified microkernel)
├── Linux Guest VM      — apps, GUI, network (routed through Tor)
├── Tor VM              — isolated Tor daemon (can't be touched by Linux)
├── Firewall PD         — Tor-only enforcement, kill switch
├── Key Vault PD        — encryption keys (never in Linux memory)
└── Boot Verifier PD    — runtime tamper detection
```

## Key Features

| Feature | How |
|---------|-----|
| **Mathematical security** | seL4 formally verified — Linux compromise can't touch keys/Tor/firewall |
| **Tor everywhere** | All traffic through Tor by default, enforced by seL4 Firewall PD |
| **Amnesic mode** | Boot from USB, use, reboot — nothing on disk (tmpfs for all writable paths) |
| **Kill switch** | Tor dies → ALL network stops instantly (enforced by PD, not Linux) |
| **MAC randomization** | New MAC address every boot |
| **Kairos AI** | JARVIS-style guardian — screen understanding, voice, active defense, local LLM |
| **Custom GUI** | Wayland compositor on wlroots 0.18, custom widget toolkit |
| **App sandboxing** | cgroups v2 + namespaces + seccomp-BPF per app |
| **A/B updates** | Atomic partition updates with rollback |

## Quick Start

### Build from Source (WSL2 Ubuntu)

```bash
# 1. Clone
git clone https://github.com/onlykushalll/WLTIOS.git
cd WLTIOS

# 2. See docs/dev-guide/build-from-source.md for full instructions
# Requires: WSL2, GCC, CMake, Ninja, QEMU, wlroots 0.18
```

### Boot in QEMU

See `docs/context/QEMU_BOOT_INSTRUCTIONS.md` for the full guide. Quick version:

```bash
# Build seL4 + Linux (32-bit for WSL2 QEMU)
cd ~/wltios-build/build-minimal-32
ninja -j$(nproc)

# Boot
qemu-system-i386 -machine pc -cpu host -m 512M -enable-kvm \
    -kernel images/kernel-ia32-pc99 \
    -initrd images/capdl-loader-image-ia32-pc99 \
    -nographic -append "console=ttyS0 init=/sbin/init"
```

Expected output:
```
========================================
=== WLTIOS Boot ===
seL4 -> Linux -> WLT-Init-Stub
========================================
[wlt-init] Starting WLT-Shell (attempt 1/5)...
WLTIOS Shell v1.0
/wlt#
```

## Documentation

| Document | Purpose |
|----------|---------|
| [Architecture](docs/ARCHITECTURE.md) | Full technical architecture |
| [Roadmap](docs/ROADMAP.md) | 24-month development roadmap |
| [Engineering Framework](docs/ENGINEERING_FRAMEWORK.md) | The 10 rules that prevent vibe-coding traps |
| [File Tree](docs/FILETREE.md) | Complete file tree for final product + honest % assessment |
| [User Guide](docs/user-guide/getting-started.md) | Installation, desktop, Tor, Kairos, troubleshooting |
| [Dev Guide](docs/dev-guide/build-from-source.md) | Build from source, testing, adding modules |
| [Security Whitepaper](docs/security/security-whitepaper.md) | Threat model, seL4 isolation, key management |
| [Audit Results](docs/security/audit-results.md) | Static analysis results (15 issues found + fixed) |
| [QEMU Boot Instructions](docs/context/QEMU_BOOT_INSTRUCTIONS.md) | Reproducible build + boot guide |
| [Privacy Policy](docs/PRIVACY.md) | What data WLTIOS collects (none) |
| [Contributing](docs/CONTRIBUTING.md) | How to contribute |

## Architecture Decisions (ADRs)

| ADR | Decision |
|-----|----------|
| [ADR-01](docs/adr/01-uefi-vs-bios.md) | UEFI boot (not BIOS) |
| [ADR-02](docs/adr/02-sel4-as-base.md) | seL4 + Linux guest VM |
| [ADR-03](docs/adr/03-tor-in-second-vm.md) | Tor in second Linux VM (not native seL4) |
| [ADR-04](docs/adr/04-microkit-libvmm.md) | Microkit for production, CAmkES for dev |
| [ADR-05](docs/adr/05-wayland-wlroots.md) | Wayland + wlroots (not X11) |
| [ADR-06](docs/adr/06-musl-libc.md) | musl libc (not glibc) |
| [ADR-07](docs/adr/07-custom-init.md) | Custom init (not systemd) |
| [ADR-08](docs/adr/08-kairos.md) | Kairos AI architecture |

## Project Status

- **Commits:** 59
- **C/H code:** ~35,000 lines
- **Tests:** 88 assertions (all pass)
- **Modules:** 24/24 have code, 10 AI modules added
- **QEMU boot:** Proven (seL4 → Linux → WLT-Shell)
- **wlroots:** 0.18.2 installed, compositor compiles + links
- **Honest completion:** ~5-8% of final production-grade OS (see [FILETREE.md](docs/FILETREE.md))

## License

GPL-3.0 — see [LICENSE](LICENSE)

## Credits

Built by **Kushal** with AI assistance (GLM-5.2, Claude, Gemini).

## Links

- **GitHub:** https://github.com/onlykushalll/WLTIOS
- **Issues:** https://github.com/onlykushalll/WLTIOS/issues
- **Security report:** See [SECURITY.md](docs/SECURITY.md)
