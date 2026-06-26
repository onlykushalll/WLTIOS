# WLTIOS Master Context — Single Source of Truth

**Last updated:** 2026-06-26 (Phase 6 complete)
**Status:** Application layer verified. seL4 capdl-loader blocker documented.

---

## What WLTIOS Is

WLTIOS (We Love Tor — Intelligent OS) is a hybrid seL4 + Linux privacy-focused operating system that routes ALL traffic through Tor. Built from scratch with a custom userspace (no systemd, static binaries, minimal RAM).

- **Microkernel:** seL4 (formally verified, capability-based)
- **Guest kernel:** Debian 5.10.0-30 (i386 32-bit + amd64 64-bit)
- **Networking:** ALL traffic through Tor. Kill switch blocks non-Tor egress.
- **Amnesic:** /var, /tmp, /run, /home are tmpfs — wiped on reboot
- **Build env:** WSL2 Ubuntu (user `kushal`, NO sudo)

---

## Current Verified State (QEMU-tested, not just "compiles")

### 32-bit Guest (Debian 5.10.0-30-686)
- ✅ seL4 + Linux boots, WLTIOS userspace as PID 1
- ✅ wlt-shell with 15 commands (ls, cat, echo, netcheck, torcheck, killswitch, sandbox, draw, etc.)
- ✅ Tor 100% bootstrap, traffic routes through SOCKS5
- ✅ Kill switch via iptables (built-in module)
- ✅ MAC randomization on every boot
- ✅ Amnesic mode — all writes in tmpfs
- ✅ App sandbox — network namespace isolation
- ✅ 10/10 integration tests pass
- ✅ 343 total test assertions pass
- ✅ RAM idle ~150MB

### 64-bit Guest (Debian 5.10.0-30-amd64)
- ✅ Tor 0.4.9.6 (glibc) — bootstraps 100%, torcheck SUCCESS
- ✅ Kill switch — route-based fallback (SIOCDELRT, no ip_tables.ko needed)
- ✅ Compositor (wlroots 0.18 headless + pixman) — exposes wl_compositor, wl_shm, wl_seat, xdg_wm_base, wl_output
- ✅ Network (e1000.ko via finit_module) + MAC randomized
- ✅ Amnesic mode
- ✅ Shell (15 commands) with Kairos tool-use loop
- ✅ wlt-terminal — connects to Wayland, creates window ("Window created, entering event loop")
- ✅ wlt-filemanager — connects to Wayland
- ✅ pkg install — hello-wlt installs + runs ("Hello from WLTIOS package manager!")
- ✅ Kairos AI — real tool_calls (ls /etc, systemctl status tor), results fed back to model
- ✅ Kairos security — 28-pattern denylist blocks dangerous commands (rm -rf / BLOCKED)

### Not Working / Blocked
- ❌ seL4 PCI passthrough — KVM boots kernel, but capdl-loader runs out of untyped memory (see SEL4_PASSTHROUGH_DEFINITIVE.md)
- ❌ /dev/fb0 — kernel lacks framebuffer driver; compositor runs headless only
- ❌ ISO creation — needs xorriso (no sudo); bootable USB package provided as alternative
- ❌ Real hardware boot — not tested

---

## Architecture

```
seL4 microkernel (formally verified)
    └── Linux VM (Debian 5.10.0-30 kernel)
        └── WLTIOS userspace (custom, static binaries)
            ├── wlt-init (PID 1 — mounts tmpfs, loads e1000.ko, starts services)
            ├── wlt-net (raw ioctl network bringup, MAC randomization)
            ├── wlt-shell (15 commands, Kairos tool use, denylist-protected)
            ├── wlt-compositor (wlroots 0.18 headless + pixman, XDG shell)
            ├── wlt-terminal (xdg-shell Wayland client, xkbcommon keyboard)
            ├── wlt-filemanager (two-panel directory browser)
            ├── wlt-crypto (Key Vault PD interface)
            ├── wlt-sandbox (unshare CLONE_NEWNET isolation)
            ├── wlt-pkg (package install/list, tar extraction)
            └── Tor 0.4.9.6 (glibc, SOCKS5 on port 9050)
```

---

## Key Technical Decisions

1. **Tor:** glibc Tor 0.4.9.6 (not Alpine musl) — avoids Zlib version mismatch. Zlib 1.3.2, Glibc 2.43.
2. **Kill switch (64-bit):** Route-based fallback via SIOCDELRT ioctl — deletes default route. No ip_tables.ko module needed. Verified: route deleted, marker saved, route restored on "off".
3. **Compositor:** Added `wlr_compositor_create()` + `wlr_shm_create_with_renderer()` — without these, wl_compositor and wl_shm globals are not advertised and clients fail with "Missing Wayland globals".
4. **Kairos tool use:** Shell sends `tools` array to llama-server. 2-turn loop: question → tool_call → execute (popen + denylist) → result → final answer. Qwen2.5-1.5B produces proper function calling.
5. **Kairos security:** 28-pattern denylist (rm -rf /, mkfs, dd, fork bombs, iptables -F, etc.) + 512-byte command length limit. Verified: "rm -rf /" → BLOCKED.
6. **seL4:** KVM required for kernel boot (TCG emulation fails at boot_sys.c:723). Capdl-loader fails at create_objects — 48,453 objects in spec, linear allocator exhausts untyped memory. Fix requires patching capdl-loader source.

---

## Build Commands

```bash
# 32-bit (all features, 10/10 tests)
bash scripts/build-initramfs.sh

# 64-bit (all features post-Phase-6)
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

# Run Kairos (needs llama-server running)
~/wltios-build/llama.cpp/build/bin/llama-server \
  -m ~/wltios-build/kairos-model.gguf --host 127.0.0.1 --port 8080 -n 2048 &
./wltios-compile64/wlt-shell
# Inside shell: kairos what files are in /etc?
```

---

## Source Layout

```
src/userspace/
  wlt-init/        — PID 1 (init-minimal.c)
  wlt-shell/       — 15-command shell with Kairos tool use (cmds/shell.c)
  wlt-net/         — network bringup, MAC randomization
  wlt-compositor/  — wlroots 0.18 headless compositor (compositor.c, comp_main.c)
  wlt-terminal/    — xdg-shell Wayland terminal client
  wlt-filemanager/ — two-panel file manager
  wlt-crypto/      — Key Vault PD interface
  wlt-sandbox/     — namespace isolation
  wlt-pkg/         — package manager
  wlt-ai/          — Kairos AI module (kairos_mcp.c, local_model.c, etc.)
  wlt-permissions/ — permission system
  wlt-render/      — rendering helpers
  wlt-settings/    — settings app (scaffold)
  wlt-tor-browser/ — Tor browser wrapper (scaffold)
  wlt-tor-mgr/     — Tor manager
  wlt-update/      — update system (scaffold)
  wlt-widgets/     — widget system (scaffold)
```

---

## Future Work

### Immediate (for GLM 5.2 builder — see WLTIOS_GUARDIAN_HANDOFF.md)
1. **WLTIOS Guardian** — Defender-class security engine (Falco + YARA + AIDE + anomaly ML)
2. **Deep-kernel MCP** — ~50 tools for Qwythos to use during investigations
3. **seL4 capdl-loader patch** — buddy allocator to fix untyped exhaustion

### Phase 7+ (not started)
- seL4 active isolation (Tor in seL4 partition, Key Vault in seL4 partition)
- Real hardware boot (KUSHALPC i5-1135G7)
- Package repository (hosted)
- 1.0 release ISO

---

## Related Docs (consolidated)

- `docs/context/SEL4_PASSTHROUGH_DEFINITIVE.md` — seL4 blocker analysis (KVM breakthrough + capdl-loader untyped exhaustion)
- `docs/context/INTEGRATION_TEST_RESULTS.md` — test results
- `docs/context/RESEARCH_NOTES.md` — merged early research (seL4, Tails, wlroots)
- `docs/adr/` — Architecture Decision Records (01-08)
- `docs/specs/` — per-module specifications
- `WLTIOS_GUARDIAN_HANDOFF.md` — master spec for the Guardian security engine build
- `PHASE5_FINAL_STATUS.md` — Phase 5 final report
