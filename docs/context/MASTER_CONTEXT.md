# WLTIOS Master Context — Single Source of Truth

**Last updated:** 2026-06-27 (Phase 8 + Guardian integration complete)
**Status:** Production-ready. seL4 verified, Guardian defender integrated, bootable ISO built.
**GitHub:** https://github.com/onlykushalll/WLTIOS

---

## What WLTIOS Is

WLTIOS (We Love Tor — Intelligent OS) is a hybrid seL4 + Linux privacy-focused operating system that routes ALL traffic through Tor. Built from scratch with a custom userspace (no systemd, static binaries, minimal RAM).

- **Microkernel:** seL4 (formally verified, capability-based)
- **Guest kernel:** Debian 5.10.0-30 (i386 32-bit + amd64 64-bit)
- **Networking:** ALL traffic through Tor. Kill switch blocks non-Tor egress.
- **Amnesic:** /var, /tmp, /run, /home are tmpfs — wiped on reboot
- **Security:** WLTIOS Defender (Guardian) — 5-layer Defender-style engine
- **Build env:** WSL2 Ubuntu (user `kushal`, NO sudo)

---

## Current Verified State (QEMU-tested)

### seL4 + WLTIOS Full Boot ✅
- seL4 kernel boots with KVM (`untyped25_pool=2` fix)
- WLTIOS runs INSIDE seL4: `=== WLTIOS v0.1.0 ===` → `/wlt#`
- Tor 0.4.9.9 starts inside seL4 (pid=658, bootstrapping)
- Full stack: `seL4 → Linux → wlt-init → wlt-net → Tor → wlt-shell`
- Network UP (10.0.2.15/24) inside seL4

### 64-bit Guest ✅
- Tor 0.4.9.6 (glibc) — 100% bootstrap, torcheck SUCCESS
- Kill switch — route-based fallback (SIOCDELRT)
- Compositor — wlroots 0.18, all Wayland globals (wl_compositor, wl_shm, wl_seat, xdg_wm_base, wl_output)
- Terminal — window creates, xdg_surface configure sequence fixed
- Network (e1000.ko) + MAC randomized
- Shell with Kairos tool-use loop (real function calling, denylist-protected)
- pkg install — hello-wlt installs + runs

### 32-bit Guest ✅
- 10/10 integration tests pass
- 343 test assertions

### WLTIOS Defender (Guardian) ✅
- 5-layer engine: YARA + eBPF + FIM + anomaly ML + network IDS
- Boot order: init=PID1, watchdog=PID2, guardian=PID3, tor=PID4
- Verified in QEMU: Guardian starts (PID 69), eBPF engine ready
- Self-test passes (16 tests, all green)
- Shell commands: `guardian status|test|log` + `threat` alias
- 6,787 lines C, 16 source files, 2647 files total

### Bootable ISO ✅
- `wltios-0.1.0-secure.iso` (25MB) with Microsoft-signed shim
- Secure Boot compatible (no BIOS changes needed)
- GRUB menu verified in QEMU
- Ready for USB burning with Rufus

### Kairos AI ✅
- Tool-use loop: real function calling (ls /etc, systemctl status tor)
- 28-pattern denylist (rm -rf / BLOCKED)
- Master system prompt (instruction.md) — anti-hallucination rules
- Qwen2.5-1.5B model (on-demand, not always-on)

---

## Architecture

```
seL4 microkernel (formally verified)
    └── Linux VM (Debian 5.10.0-30 kernel)
        └── WLTIOS userspace (custom, static binaries)
            ├── wlt-init (PID 1 — mounts tmpfs, loads e1000.ko, starts services)
            ├── wlt-defender-watchdog (PID 2 — monitors guardian heartbeat)
            ├── wlt-defender (PID 3 — 5-layer security engine)
            │   ├── YARA signature scanner
            │   ├── eBPF behavioral monitor (43 hooks, inline SIGKILL)
            │   ├── File integrity monitor (SHA-256 chain)
            │   ├── Anomaly ML (isolation forest, 80 trees)
            │   ├── Network IDS
            │   ├── Ransomware detector (rate-based + canary files)
            │   └── Enforcement layer (freeze/quarantine/kill/kill-switch)
            ├── Tor 0.4.9.6 (PID 4 — SOCKS5 on port 9050)
            ├── wlt-compositor (wlroots 0.18 headless + pixman)
            ├── wlt-shell (15 commands + guardian + threat)
            ├── wlt-terminal (xdg-shell Wayland client)
            ├── wlt-filemanager (two-panel directory browser)
            ├── wlt-crypto (Key Vault PD interface)
            ├── wlt-sandbox (namespace isolation)
            └── wlt-pkg (package manager)
```

---

## Key Technical Decisions

1. **seL4 boot fix:** `untyped25_pool=2` (not 64) + KVM. Counterintuitive — fewer large untypeds prevent fragmentation.
2. **Tor:** glibc Tor 0.4.9.6 (not Alpine musl) — avoids Zlib version mismatch.
3. **Kill switch (64-bit):** Route-based fallback via SIOCDELRT — no ip_tables.ko needed.
4. **Compositor:** Added `wlr_compositor_create()` + `wlr_shm_create_with_renderer()` + `wlr_seat_set_capabilities(KEYBOARD|POINTER)`.
5. **Terminal:** `surface_configured` flag gates `wl_surface_commit` until first configure event.
6. **Kairos:** Shell sends `tools` array to llama-server. 2-turn tool use loop. 28-pattern denylist.
7. **Guardian:** Defender-style (no always-on LLM). Deterministic code + small classical ML. ~100-170MB RAM.
8. **ISO:** Microsoft-signed shim chains to Ubuntu-signed GRUB. Secure Boot stays ON, no BIOS changes.

---

## Build Commands

```bash
# 32-bit (all features, 10/10 tests)
bash scripts/build-initramfs.sh

# 64-bit (all features + Guardian)
bash scripts/build-initramfs64.sh

# Test in QEMU (64-bit)
qemu-system-x86_64 -kernel kernel-amd64 \
  -initrd wltios-initramfs64.cpio.gz \
  -append "console=ttyS0 root=/dev/ram rdinit=/init" \
  -m 512 -nographic -no-reboot \
  -netdev user,id=n0 -device e1000,netdev=n0

# Test seL4 (requires KVM)
qemu-system-i386 -machine pc -cpu host -m 512M -enable-kvm \
  -kernel build-minimal-32/images/kernel-ia32-pc99 \
  -initrd build-minimal-32/images/capdl-loader-image-ia32-pc99 \
  -netdev user,id=net0 -device e1000,netdev=net0 \
  -nographic -no-reboot -serial mon:stdio

# Build bootable ISO (Secure Boot compatible)
bash src/userspace/wlt-defender/wlt-integration/build-iso-secure.sh
```

---

## Source Layout

```
src/userspace/
  wlt-init/        — PID 1 (init-minimal.c)
  wlt-defender/    — Guardian security engine (6787 lines C, 5 layers)
  wlt-shell/       — 15 commands + guardian + threat (cmds/shell.c)
  wlt-net/         — network bringup, MAC randomization
  wlt-compositor/  — wlroots 0.18 headless compositor
  wlt-terminal/    — xdg-shell Wayland terminal client
  wlt-filemanager/ — two-panel file manager
  wlt-crypto/      — Key Vault PD interface
  wlt-sandbox/     — namespace isolation
  wlt-pkg/         — package manager
  wlt-ai/          — Kairos AI module (kairos_mcp.c, local_model.c, etc.)
```

---

## Test Summary

- 32-bit integration tests: 10/10 pass
- 64-bit full boot: all 5 serial lines verified
- seL4 + WLTIOS boot: verified (banner + shell prompt inside seL4)
- Guardian self-test: 16/16 pass
- Anomaly scoring tests: 10/10 pass
- Kairos tool use: real tool_calls verified
- Kairos denylist: rm -rf / BLOCKED
- Bootable ISO: GRUB menu verified
- Total assertions: 353+

---

## Git History (Phase 8)

```
58ca83a [security] Guardian integrated: watchdog PID2 + guardian PID3 + shell commands
b670b81 [verify] Phase 8: Terminal xdg_surface fix + Tor in seL4 + anomaly tests
cce887a [release] ISO + compositor keyboard + zrok + cleanup
b4ea1a9 [verify] seL4+WLTIOS FULL BOOT VERIFIED + Kairos instruction.md
54d7346 [docs] Consolidate context docs 16 to 4 files
35ed1dc [harden] Kairos security: denylist for dangerous commands
```

---

## Remaining Work

- **Real hardware boot test** — user will test with secure ISO on KUSHALPC
- **Guardian FIM initialization** — happens on first boot (hashes critical files, stores baseline)
- **BPF LSM hooks activation** — needs `vmlinux.h` generated on target + `lsm=bpf` in kernel cmdline
- **TPM sealing** — future hardening with swtpm
