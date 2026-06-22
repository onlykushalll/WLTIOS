# WLTIOS Website Content

## Homepage

### Hero Section
**WLTIOS** — The OS that combines Windows usability, Linux efficiency, and Tails anonymity.

**Tagline:** Privacy is not optional. Security is not optional. Your OS should be both.

### Features

#### seL4 Isolation
Mathematically proven security. Even if Linux is compromised, your keys, Tor, and firewall are safe.

#### Tor Everywhere
All traffic routes through Tor by default. No configuration needed. Kill switch prevents leaks.

#### Amnesic Mode
Boot from USB, use the OS, reboot — everything is gone. No trace on disk.

#### Kairos AI
A built-in JARVIS-style guardian. Sees your screen, hears your voice, protects your system.

#### Custom UI
A Wayland compositor built from scratch. Not GNOME, not KDE — WLTIOS.

### Download
- Live USB ISO (coming soon)
- Source code: https://github.com/onlykushalll/WLTIOS

### Get Involved
- **GitHub**: https://github.com/onlykushalll/WLTIOS
- **Issues**: Report bugs and request features
- **Contributing**: See CONTRIBUTING.md
- **Forum**: (coming soon)

## About Page

WLTIOS is built on:
- **seL4** microkernel (formally verified)
- **Linux** guest VM (hardware compatibility)
- **wlroots** 0.18 (Wayland compositor)
- **musl** libc (lightweight, clean)
- **Tor** (anonymity)

Started June 2026. Built by Kushal with AI assistance.

## Security Page

Read our [Security Whitepaper](../security/security-whitepaper.md) for the full architecture.

Key guarantees:
- seL4 formal verification (mathematical proof of isolation)
- Tor-only networking (enforced by seL4 Firewall PD)
- Amnesic mode (RAM-only, no disk writes)
- Key Vault PD (keys never in Linux memory)
- Boot Verifier PD (tamper detection)
