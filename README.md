# WLTIOS — We Love Tor, Intelligent OS

A hybrid seL4 + Linux privacy-focused operating system that routes ALL traffic through Tor. Built from scratch with a custom userspace, formally verified microkernel, and a Defender-class security engine.

**GitHub:** https://github.com/onlykushalll/WLTIOS

---

## What WLTIOS Does

- **seL4 verified boot** — runs on a formally verified microkernel, not bare Linux
- **Tor anonymity** — all network traffic routes through Tor (SOCKS5 on port 9050)
- **Kill switch** — non-Tor egress blocked via route deletion (SIOCDELRT)
- **Amnesic** — all writes in tmpfs (RAM), wiped on reboot
- **Defender engine** — 5-layer security: YARA + eBPF + FIM + anomaly ML + network IDS
- **MAC randomization** — new MAC on every boot
- **App sandbox** — network namespace isolation
- **Kairos AI** — on-demand security consultant with tool use (denylist-protected)
- **Bootable ISO** — Secure Boot compatible (Microsoft-signed shim)

---

## Architecture

```
seL4 microkernel (formally verified)
    └── Linux VM (Debian 5.10.0-30)
        └── WLTIOS userspace
            ├── wlt-init (PID 1)
            ├── wlt-defender-watchdog (PID 2)
            ├── wlt-defender (PID 3 — 5-layer security engine)
            ├── Tor (PID 4)
            ├── wlt-compositor (wlroots 0.18)
            └── wlt-shell (15+ commands)
```

---

## Quick Start

### Build

```bash
# 32-bit (all features, 10/10 tests)
bash scripts/build-initramfs.sh

# 64-bit (all features + Guardian)
bash scripts/build-initramfs64.sh
```

### Test in QEMU

```bash
# 64-bit guest
qemu-system-x86_64 -kernel kernel-amd64 \
  -initrd wltios-initramfs64.cpio.gz \
  -append "console=ttyS0 root=/dev/ram rdinit=/init" \
  -m 512 -nographic -no-reboot \
  -netdev user,id=n0 -device e1000,netdev=n0

# seL4 (requires KVM)
qemu-system-i386 -machine pc -cpu host -m 512M -enable-kvm \
  -kernel build-minimal-32/images/kernel-ia32-pc99 \
  -initrd build-minimal-32/images/capdl-loader-image-ia32-pc99 \
  -netdev user,id=net0 -device e1000,netdev=net0 \
  -nographic -no-reboot -serial mon:stdio
```

### Bootable ISO

The ISO at `build/wltios-0.1.0-secure.iso` is Secure Boot compatible (uses Microsoft-signed shim). Burn with Rufus to a USB stick and boot.

---

## WLTIOS Defender (Guardian)

A Defender-style endpoint protection engine — deterministic code + small classical ML, no always-on LLM.

**5 layers:**
1. **YARA signatures** — known malware pattern matching
2. **eBPF behavioral rules** — 43 syscall hooks, inline SIGKILL enforcement
3. **File integrity monitor** — SHA-256 chain, tamper detection
4. **Anomaly ML** — isolation forest (80 trees, 8 features)
5. **Network IDS** — non-Tor egress detection

**Enforcement tiers:**
- Reversible (freeze, quarantine, block-net) → autonomous
- Irreversible (kill, delete, kill-switch) → human confirm required

**Shell commands:** `guardian status`, `guardian test`, `guardian log`, `threat`

---

## Kairos AI

On-demand security consultant (not always-on). Uses Qwen2.5-1.5B via llama.cpp.

```bash
kairos what files are in /etc?
# → [Tool call detected] run_command: ls /etc
# → [Tool result]: PackageKit X11 adduser.conf ...
# → Kairos: The contents of /etc directory vary...
```

Protected by a 28-pattern denylist — `rm -rf /` is BLOCKED.

---

## Verified State

| Feature | Status |
|---|---|
| seL4 + WLTIOS boot | ✅ Verified (untyped25_pool=2 + KVM) |
| Tor inside seL4 | ✅ Tor 0.4.9.9 starts (pid=658) |
| Bootable ISO | ✅ 25MB, Secure Boot, GRUB verified |
| Guardian | ✅ Integrated, PID 3, self-test passes |
| Terminal | ✅ Window creates, xdg_surface fixed |
| Kairos tool use | ✅ Real function calling |
| pkg install | ✅ hello-wlt installs + runs |
| Kill switch | ✅ Route-based fallback |
| Anomaly tests | ✅ 10/10 pass |
| Integration tests | ✅ 10/10 (32-bit) |
| Test assertions | 353+ |

---

## Documentation

- `docs/context/MASTER_CONTEXT.md` — single source of truth
- `docs/context/SEL4_PASSTHROUGH_DEFINITIVE.md` — seL4 fix analysis
- `docs/context/INTEGRATION_TEST_RESULTS.md` — test results
- `docs/context/PROGRESS_LOG.md` — historical progress
- `docs/adr/` — Architecture Decision Records (01-08)
- `docs/specs/` — per-module specifications
- `docs/specs/kairos-instruction.md` — Kairos master system prompt
- `src/userspace/wlt-defender/README.md` — Guardian documentation

---

## License

GPL-2.0 (seL4 components) + project-specific for userspace

## Authors

Kushal + AI assistance (Z.ai Code, Claude, GLM)
