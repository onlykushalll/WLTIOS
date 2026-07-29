# WLTIOS Project — Complete Context Summary

> **Source:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\wholecontext.txt` (32,184 lines, full Z.ai/GLM ↔ Kushal chat export from 2026-06-18 onward).
> **Purpose:** Reconstruct full project state for a fresh session without re-reading the huge file.
> **Compiled by:** SUMMARIZE-CHAT subagent.

---

## 1. Project Overview

**WLTIOS** ("Windows + Linux + Tails + Inspired OS") is a privacy-first, anonymity-oriented general-purpose desktop OS being built from scratch with AI assistance. Core pitch:

- **Base layer:** seL4 microkernel (formally verified) running Linux as a guest VM for driver compatibility.
- **Anonymity layer:** Tor routed through every network call, with a route-based kill switch that halts all traffic if Tor drops.
- **Security layer:** "Guardian" (a.k.a. `wlt-defender`) — PID 3 daemon combining YARA malware scanning, BPF hooks, FIM (file integrity monitor), anomaly ML, canary files, and an enforcement engine (kill switch, process freeze, lockdown).
- **AI layer:** "Kairos" — on-device assistant built around a local LLM (currently Qwen2.5-1.5B as a stand-in for the intended Qwythos-9B / VibeThinker-3B). Kairos uses a tool-call protocol with a security denylist (e.g. `rm -rf /` blocked).
- **Target hardware:** Dell Latitude 3420 named KUSHALPC (Intel i5-1135G7, 8 GB RAM, Windows 10).
- **Target metrics:** <256 MB idle RAM, <3 s boot, fully amnesic (tmpfs root, RAM wipe on shutdown), MAC spoofing, no telemetry.

### Architecture stack (top-to-bottom)
| Layer | Component |
|---|---|
| Boot | `wlt-boot` (UEFI loader, SHA-256 + Ed25519 verified boot, monocypher-backed) |
| Kernel | seL4 (verified base) + Linux guest (driver/world model) |
| Init | `wlt-init` (custom init, dependency graph, service monitor, amnesic mount, Tor launcher) |
| Compositor | `wlt-compositor` (wlroots-based, headless/pixman + DRM backends intended) |
| Shell/UI | `wlt-shell` (taskbar, launcher, notifications, tray), `wlt-terminal`, `wlt-filemanager`, `wlt-settings` |
| Network | `wlt-net` (eth0/wifi bringup, kill switch, per-PID isolation, MAC spoof, Tor SOCKS5 proxy) |
| Security | `wlt-guardian` (YARA + BPF + FIM + anomaly + canary + enforcement), `wlt-sandbox`, `wlt-confirm` (constant-time confirm token) |
| Crypto | `wlt-crypto` (LUKS2/argon2id, file_crypto, secure_delete, keyvault client) |
| seL4 PDs | `key-vault-pd` (SHA-256, HMAC-DRBG, XSalsa20-Poly1305 AEAD, PBKDF2, FNV-1a keystore, tamper latch), `boot-verifier-pd` (spec only) |
| AI | `wlt-ai` (Kairos CLI, MCP tool interface, instruction.md master prompt, denylist) |
| Package | `wlt-pkg` (Nix-style immutable store, A/B Btrfs snapshots, Tor-only downloads, Ed25519+TUF signing) |

---

## 2. People / Roles

| Role | Identity | Where they work |
|---|---|---|
| **Kushal** (developer/user) | Human, owns KUSHALPC | Drives all sessions, decides scope, runs MCPilot on PC |
| **Claude** (AI #1) | Anthropic Claude with **Desktop Commander MCP** + **Filesystem MCP** on KUSHALPC | Works directly against `~/wltios-build/` inside WSL2 Ubuntu on the Dell — real seL4/CAmkES/QEMU/guardian/compositor. Issues the big multi-hour task prompts. |
| **GLM / Z.ai** (AI #2) | This cloud sandbox (`/home/z/my-project/`) — a Next.js dev environment with bash/Read/Write/Grep/etc. tools, but **no** QEMU, **no** seL4, **no** MCPilot bridge unless Claude's session is alive | Builds skeleton code, writes specs/ADRs/tests, audits, mirrors code to `/home/z/my-project/wltios/`. Cannot reach the real WSL2 tree. |
| **MCPilot** | Kushal's homegrown MCP server (`mcpilot.cjs`, 53 tools, pure Node) running on KUSHALPC port 7878, exposed via Cloudflare Tunnel at `https://kushalneedsmcp.online` | The bridge GLM uses to read/write Windows files; `node /home/z/my-project/mcp-call.js <tool> '<json>'` is the only sanctioned call path |
| **Hardware** | Dell Latitude 3420, KUSHALPC, i5-1135G7, 8 GB RAM, Windows 10 | All builds target this. Real hardware boot never achieved — ISO sits on desktop awaiting user test. |

---

## 3. Complete Phase History (chronological)

### Phase 0 — MCPilot + tooling bootstrap (chunks 1–4)
- Initial chat: GLM explains its built-in tools vs Desktop Commander.
- Bash tools broke (403 cascade). User set up Cloudflare Tunnel (`cloudflared.exe`) → first tunnel URL `assure-associates-contributor-medicaid.trycloudflare.com`, later permanent `https://kushalneedsmcp.online`.
- Built MCPilot v2 (53 tools, Bearer auth `mcpilot-secret-2024`), 314 KB/s throughput vs DC's 3,600 c/s @ 50% fail.
- Read all 200 `C:\tmp` files (1.34 MB in 4.4 s).
- **Honest flag:** the "VBS bug fixed" report was a hardcoded template string; the real VBS files (`start-mcpilot-v2.vbs`, `start-mcpilot.vbs`) in `_archive\` were never touched.

### Phase 1 — OS research + architecture (chunks 4–6)
- Two parallel researcher subagents produced 3,813 lines of OS research with 36 sources.
- Critical finding: **Tor cannot run natively in seL4** (needs libc/sockets/fs) → ADR-03: Tor in second Linux VM.
- Critical finding: **GPU passthrough on seL4 never publicly demonstrated** — biggest risk.
- Wrote 7 ADRs (UEFI vs BIOS, seL4 as base, Tor-VM, Microkit, Wayland, musl, custom init).
- Wrote ARCHITECTURE.md (619 lines), PRD.md (501), CODING_STYLE.md, MODULE_MAP.md, ROADMAP.md.
- Pushed initial commits to `github.com/onlykushalll/WLTIOS` (commit `e2a507c` = 4 files, 1324 insertions; `ba257b3` = 12 docs).

### Phase 2 — WSL2 + seL4 bring-up (chunks 5–6)
- Tried MSYS2 build of seL4 → blocked (`umm_types.txt` cache issue, seL4 not Windows-friendly).
- Installed WSL2 Ubuntu 26.04 (one reboot for Hyper-V); cloned seL4 + camkes-vm-examples via `repo`.
- Built seL4 + Linux guest image (`images/capdl-loader-image-x86_64-pc99`).
- QEMU boot hit two blockers in WSL2 nested KVM:
  - TCG (software emulation) doesn't support PCID (seL4 requires it for x86_64).
  - WSL2 KVM doesn't expose `vmx-apicv-xapic` (APIC Virtualization) → 64-bit kernel won't boot under nested virtualization.
- **Workaround:** build 64-bit fully (ready for bare metal), use 32-bit (i386) for QEMU testing in WSL2.
- Verified SHA-256 implementation against 4 NIST FIPS 180-4 test vectors.
- Commit `f46973a` pushed.

### Phase 3 — 64-bit boot + compositor + Kairos first words (chunks 9–10)
- **Commit `97025b5`** — Network layer + Tor in 32-bit guest:
  - Real eth0 via raw ioctl() in `qemu_bringup.c` (350 lines) — loopback + eth0 + default route + DNS, no external `ip`/`iptables`.
  - Tor 0.4.9.9 (Alpine musl i386) bundled with 9 shared libs, runs via `/lib/ld-musl-i386.so.1 --library-path /lib /usr/bin/tor`.
  - Verified end-to-end: `Bootstrapped 100% (done)` + SOCKS5 connect to `check.torproject.org` returning HTTP 301.
  - 3 new shell commands: `netcheck`, `tstatus`, `torcheck`.
  - `cpio --owner=root:root` fix for Tor data dir ownership.
  - Honest disclosure: booted Linux kernel directly in QEMU (bypassed seL4) because seL4's `minimal.camkes` doesn't enable PCI passthrough for the NIC.
- **Commit `1800294`** — 64-bit kernel boots in QEMU; compositor running in 64-bit guest; `wayland-0` socket live.
- Kairos first words verified: "Hello! I'm Kairos, the AI assistant built into the WLTIOS operating system." (on Qwen2.5-1.5B stand-in, not Qwythos-9B).
- 248 test assertions (target ≥160), 14 test suites, 0 failures.

### Phase 4 — Tests, LUKS2, more polish (chunk 10)
- New tests: `terminal_test.c`, `pkg_test.c`, `kairos_tools_test.c`, `luks_test.c` (validates `cryptsetup luksFormat --type luks2 --pbkdf argon2id`).
- **321 assertions** (target ≥280), 18 test suites, 0 failures.
- **Commit `2f035d0`** — initramfs expanded (4 binaries, shell responds to input, services start).
- **Commit `0e7b6e4`** — boot/SHA-256/Ed25519 skeleton code.
- Kairos security denylist: `rm -rf /` → BLOCKED.

### Phase 5 — Verification sprint + 64-bit Tor fix (chunks 10–11)
- Tried and failed to make 64-bit guest run Tor + iptables (Alpine x86_64 library symlink issues: `libssl.so.3`, `libcrypto.so.3`, `libz.so.1`, `liblzma.so.5`, `libzstd.so.1`, `libcap.so.2`, `libip6tc` — all downloaded + symlinked).
- **Honest root cause:** Alpine x86_64 packaging has different symlink structures than i386. e1000 driver not built into amd64 Debian kernel. iptables binary not in 64-bit initramfs.
- 32-bit guest: 10/10 integration tests pass.
- 150.8 MB RAM idle (well under 256 MB target).
- **Commit `61acc1a`** — bootable package + integration test suite.
- **Commit `ae28533`** — 64-bit Tor status documented honestly.
- **Commit `4a52fef`** — fixes pushed.

### Phase 6 — seL4 untyped fix + Tor inside seL4 (chunks 11–12)
- Solved the "Insufficient memory (1 * 8388608 bytes needed, 0 bytes available)" error: added `simple_untyped25_pool=2` to `minimal.camkes`.
- **Commit `b4ea1a9`** — WLTIOS runs **inside seL4** with `/wlt#` shell prompt. Serial output: `=== WLTIOS v0.1.0 ===`. This is the first genuine seL4+WLTIOS full boot.
- Kairos `instruction.md` master prompt uploaded (committed in `b4ea1a9`).
- **Commit `35ed1dc`** — Kairos security denylist for dangerous commands + command length limit.
- Kairos tool use verified end-to-end: `kairos what files are in /etc?` → tool call `ls /etc` → real listing; `kairos is tor working right now?` → tool call `systemctl status tor` → result fed back.
- Bug fixed: tool-call parser had a `strstr(tool_call_pos, "command")` bug matching `command` inside the function name `run_command`; fixed to search for the `arguments` field first.
- `pkg install /packages/hello-wlt-1.0.0-wlt.tar.gz` works end-to-end.

### Phase 7 — Guardian defender (chunks 12–13)
- Built `wlt-guardian` (a.k.a. wlt-defender):
  - 5 engines: eBPF, YARA, FIM, anomaly (lightweight ML), canary files.
  - Enforcement actions: kill switch, process freeze (SIGSTOP), lockdown.
  - PID 3 in the boot order.
  - YARA rules: 10+ covering reverse shells, privilege escalation, crypto miners, data exfiltration, rootkit signatures, ransomware, persistence.
  - FIM baseline: SHA-256 of `/sbin/init`, `/bin/wlt-shell`, `/usr/bin/wlt-compositor`, `/usr/bin/tor`, `/etc/tor/torrc`, `/etc/wltios-release`, `/lib/ld-linux-x86-64.so.2`.
- **Commit `cce887a`** — ISO + compositor keyboard + zrok tunnel + cleanup.
- ISO on desktop: `wltios-0.1.0-secure.iso` (34 MB, signed Microsoft shim, has Guardian + unified killswitch).

### Phase 8 — Real bug-hunting + kill switch + self-freeze (chunks 14–15)
- **Close 1 (socket hang):** Guardian SIGSTOP'd itself → watchdog timeout → kill switch activation → guardian dead. Root cause: BPF hooks detected Tor's file operations as ransomware behavior (`BULK_DELETE_SUSPICIOUS` / `RENAME_STORM_SUSPICIOUS` / `RANSOMWARE_IO_PATTERN` rules fired) → action engine AUTO FREEZE'd the process.
  - Fix A: `g_action_freeze(): if (pid == getpid()) return 0;` (self-freeze prevention).
  - Fix B: ransomware rules downgraded from FREEZE to LOG severity.
- **Close 2 (killswitch off bug):** `g_action_kill_switch()` didn't distinguish on/off — always activated. Then later, after on/off parsing added, `g_action_kill_switch_deactivate()` returned `rc=-1` because the activate function didn't save the gateway before `SIOCDELRT`.
  - Fix: read default gateway from `/proc/net/route` BEFORE deleting, save to `/run/wlt-killswitch`. After fix, `killswitch off` returns `rc:0`.
- **Commits `b5988fe` (killswitch off fix) + `df0c844` (self-freeze fix)** — but ISO on desktop (Jul 2) is STALE; doesn't include these.
- Built real `vmlinux.h` from the running kernel, BPF objects load → 23 BPF hooks live, YARA detection fires, FIM detects tampering + auto-restores.
- Guardian `--self-test` passes 100/100.
- **Honest flag:** the killswitch sequence was run multiple times with conflicting results across messages — eventually traced to the self-freeze bug.
- 23 BPF hooks loaded but **BPF cross-segment / CO-RE issue:** programs need real `vmlinux.h` from the target kernel (build-time step, not runtime bug).
- **Commit `1c6b3a5`** — KeyVaultPD files committed (but commit message mismatches diff — see §12).

### Phase 9 — seL4 Multi-PD + sandbox work + verification (chunks 15–16)
- Built `key-vault-pd` C sources brought into main project at `src/sel4-partitions/key-vault-pd/`:
  - Full crypto: SHA-256, HMAC-DRBG, XSalsa20-Poly1305 AEAD, PBKDF2, volatile keystore with FNV-1a open addressing, tamper latch.
  - Enhanced Microkit host shim — fixed per-TU-static bug where message-register array was duplicated across translation units.
  - Microkit System Description (`wltios.system`) — defines multi-PD topology.
- **26/26 host simulation checks pass** (boot zeroes state, ping→pong MR0=3, generate→encrypt→decrypt AEAD round-trip, tamper-reject on bit-flip, tamper-latch self-immolation, status PPC).
- QEMU harness `scripts/qemu-multi-pd.sh` boots the full seL4 system when Microkit SDK is installed; gracefully falls back to host simulation otherwise (ADR-04 de-risking step).
- **Commit `066b97a`** — settings VLM screenshot path closed (wlt-screenshot utility, test-fb-gen, vlm-screenshot.sh, compositor export spec).
- **Audit of sandbox mirror** found 4 critical bugs (see §5):
  1. `wlt_verify_ed25519` (verify.c:246-263) was a fail-open stub returning `WLT_BOOT_OK` unconditionally.
  2. `wlt_verify_kernel` (verify.c:231-233) does raw SHA-256 hash-compare, not signatures.
  3. Loader (line 412) only calls `wlt_loader_verify_elf()` — neither verify function is wired in.
  4. Client `wlt-crypto/keyvault.c` speaks Unix socket; seL4 Key Vault PD speaks Microkit PPC. No bridge exists.
- **Hand-rolled Ed25519 → 9/9 RFC 8032 vectors pass** — but Claude rejected this as "don't roll your own crypto"; specified monocypher integration instead.
- **Monocypher swap → 12/12 pass**, then independently cross-checked against PyNaCl (libsodium): 8/8 cases agree. Confirmed the wrapper is `crypto_ed25519_check` (Ed25519-compatible), NOT `crypto_eddsa_check` (different hash-to-scalar). Polarity correct (rc==0 → valid). Recorded in `docs/context/ED25519_CROSSCHECK.md`.
- **Confirm-token** (constant-time): 12/12 tests pass.
- **Per-PID network isolation** (rule generation + policy): 69/69 tests pass. Wired into `net.h` via `#include "pid_net.h"` so it composes with kill_switch.
- **RAM wipe on shutdown**: 7/7 tests pass — wiped 253 MiB across 15 chunks, idempotent. Honest limit: can't wipe kernel-space keys (LUKS master key in dm-crypt) — needs kexec wipe-kernel (future work). Spec at `docs/specs/wlt-ram-wipe.md`.
- Real seL4 multi-component work already existed at `~/wltios-build/projects/vm-example/apps/x86/keyvault_pd/` (dated Jul 4). Boot reached `[key-vault-pd] alive` independent of Linux but **ping/pong NOT yet verified** — rootfs booting was the CAmkES example's own minimal test image, not WLTIOS's; no shell to trigger ping from Linux side. Non-fatal "Insufficient memory (8388608 bytes)" / "4194304 bytes" warnings remain unexplained.

---

## 4. Current Verified State (honest)

| Claim | Status | Real evidence |
|---|---|---|
| seL4 + WLTIOS + Tor full boot in QEMU (32-bit) | ✅ Verified | Serial: `=== WLTIOS v0.1.0 ===` → `/wlt#` inside seL4; `Bootstrapped 100% (done)`; HTTP 301 from `check.torproject.org` |
| 10/10 integration tests pass (32-bit) | ✅ Verified | PID ordering, Tor UID detection, BPF attached, canary files exist, FIM baseline covers passwd+shadow+torrc, kill switch socket→BPF map |
| 64-bit guest boots in QEMU | ✅ Verified | All 5 serial lines |
| 64-bit Tor/iptables | ❌ Broken | Alpine x86_64 library symlink mismatch, no e1000 driver in amd64 Debian kernel |
| RAM idle 150.8 MB | ✅ Verified (32-bit) | `< 256MB target` |
| Compositor running in QEMU guest, `wayland-0` socket live | ✅ Verified | But only headless/pixman backend — see next row |
| Compositor DRM backend (visible GUI) | ❌ Never achieved | Windows exist in Wayland memory, never reach `/dev/fb0` |
| Terminal / filemanager / settings "create windows" | ⚠️ Protocol-level only | Logs say `Window created, entering event loop` — no human has ever seen a window |
| Kairos first words | ✅ Verified | "Hello! I'm Kairos, the AI assistant built into the WLTIOS operating system." (on Qwen2.5-1.5B stand-in, NOT Qwythos-9B) |
| Kairos tool use end-to-end | ✅ Verified | `ls /etc` and `systemctl status tor` both produced real output via tool calls |
| Kairos security denylist | ✅ Verified | `rm -rf /` blocked (commit `35ed1dc`) |
| `pkg install hello-wlt` | ✅ Verified end-to-end | Installs + runs + listed |
| Ed25519 verified boot (monocypher-backed) | ✅ 12/12 vectors + PyNaCl cross-check | Real kernel signing + bootloader boot NOT yet tested |
| SHA-256 (FIPS 180-4) | ✅ 4/4 NIST vectors | |
| Guardian `--self-test` | ✅ 100/100 | With two honest flags: anomaly hash-mismatch noise; legacy `MCP_AUTOCONFIRM` confirm-token gap confirmed live |
| Guardian 23 BPF hooks live | ✅ Verified | Real vmlinux.h built from running kernel; BPF objects load |
| YARA detection (MALICIOUS verdict) | ✅ Verified | |
| FIM tamper detection + auto-restore | ✅ Verified | |
| Kill switch on/off (unified via guardian socket) | ✅ Verified after fixes | `killswitch on` → `{"status":"ok","action":"kill_switch_on","rc":0}`; `off` → `rc:0` after gateway-save fix |
| Self-freeze prevention | ✅ Fixed | `if (pid == getpid()) return 0;` |
| Key Vault PD host simulation | ✅ 26/26 pass | NOT real seL4 isolation |
| Key Vault IPC bridge | ✅ 32/32 host sim | NOT real vchan — pluggable-transport abstraction with stub |
| Confirm-token (constant-time) | ✅ 12/12 | NOT wired into real `guardian.c` — real guardian still uses `MCP_AUTOCONFIRM` deprecated fallback |
| Per-PID network isolation | ✅ 69/69 | NOT touched real `guardian.c`; exists only in GLM sandbox mirror |
| RAM wipe on shutdown | ✅ 7/7 | Userspace only; can't wipe kernel-space keys (LUKS master) — needs kexec |
| Bootable ISO | ✅ Built | `wltios-0.1.0-secure.iso` (34-35 MB, signed Microsoft shim) on desktop |
| Real hardware boot on Dell | ❌ Never tested | User deferred — ISO sits on desktop awaiting manual USB test |
| seL4 multi-PD: Key Vault PD boots independent of Linux | ⚠️ Partially | `[key-vault-pd] alive` seen, but ping/pong NOT yet exchanged |
| Kairos Qwythos-9B upgrade | ❌ Deferred | User forbade AI model downloads on their RAM |

---

## 5. Key Bugs Found & Fixed (with file:line where known)

| # | Severity | Location | Bug | Fix |
|---|---|---|---|---|
| 1a | CRITICAL | `verify.c:246-263` | `wlt_verify_ed25519` was a fail-open stub — ignored all inputs, returned `WLT_BOOT_OK` unconditionally. Silent bypass. | FIXED → returns `WLT_BOOT_ERR_SIGNATURE` (fail-closed), then replaced with real monocypher `crypto_ed25519_check` |
| 1b | CRITICAL | `verify.c:231-233` | `wlt_verify_kernel` does raw SHA-256 hash-compare, not signatures. Attacker who can write the kernel can write the hash file too. | FIXED → replaced with real call to `wlt_verify_ed25519()` |
| 1c | CRITICAL | `loader.c:412` | Loader only calls `wlt_loader_verify_elf()` (ELF format check) — neither verify function actually wired into boot path. | FIXED (sandbox) — wiring into loader. NOT yet verified on real QEMU boot |
| 2 | ARCHITECTURE | `wlt-crypto/keyvault.c` ↔ `sel4-partitions/key-vault-pd/keyvault.c` | Client speaks Unix socket `/run/key-vault.sock`; server PD speaks Microkit PPC. No bridge code anywhere (`grep -rln vchan\|virtio_serial\|kv_bridge` = empty). | Documented — this is the next task. Bridge built in sandbox but is pluggable-transport abstraction with stub, NOT real vchan |
| 3 | CRITICAL (runtime) | `action_engine.c` `g_action_freeze()` | Self-freeze deadlock: Guardian SIGSTOP'd itself (pid=69) via ransomware false positive on Tor file ops → heartbeat stopped → watchdog activated kill switch → guardian dead. | FIXED → `if (pid == getpid()) return 0;` + ransomware rules downgraded from FREEZE to LOG |
| 4 | MAJOR | `action_engine.c` `g_action_kill_switch()` | Kill switch off command didn't distinguish on/off — always activated. Then `g_action_kill_switch_deactivate()` returned `rc=-1` because activate didn't save gateway before `SIOCDELRT`. | FIXED → read default gateway from `/proc/net/route` BEFORE deleting, save to `/run/wlt-killswitch`. `killswitch off` now returns `rc:0` |
| 5 | MAJOR | Kairos `kairos.c` tool-call parser | `strstr(tool_call_pos, "command")` matched `command` inside the function name `run_command`, not the arguments field. Also JSON had escaped quotes (`\"command\"`) the parser didn't handle. | FIXED → search for `arguments` field first, handle escaped quotes, add closing brace for `args_field` wrapper |
| 6 | MINOR | `microkit.h` host shim | Per-TU-static bug — message-register array was duplicated across translation units. PPC round-trips silently returned zero. | FIXED → made extern shared storage |
| 7 | ARCHITECTURE | `minimal.camkes` | "Insufficient memory (1 * 8388608 bytes needed, 0 bytes available)" during seL4 boot — IOSpace caps couldn't be allocated. | FIXED → added `simple_untyped25_pool=2` |
| 8 | DOCS | `docs/adr/04-microkit-libvmm.md` | ADR said "Microkit + libvmm, do not use CAmkES" but actual working build uses CAmkES (`camkes-vm-examples`). Contradiction never updated. | FIXED → "Amendment (2026-06-22)" section documents actual CAmkES usage + Microkit transition plan |
| 9 | BUILD | `wlt-init/*.c` | Claude found malloc references in `wlt-init` despite `CODING_STYLE.md` forbidding heap allocations. | FIXED → all heap allocations replaced with static buffers |
| 10 | MINOR | Guardian ransomware rules | `BULK_DELETE_SUSPICIOUS` / `RENAME_STORM_SUSPICIOUS` / `RANSOMWARE_IO_PATTERN` fired false-positive on Tor's normal file operations. | FIXED → downgraded from FREEZE to LOG severity |
| 11 | HONEST | Sandbox `wlt_verify_ed25519` hand-rolled impl | Multiple bugs found during development: wrong `d_bytes`, incorrect `fe_sq` coefficients, off-by-one in reduction (`t[i-15]` should be `t[i-16]`), non-converging carry propagation. | Replaced with monocypher per Claude's directive. The hand-rolled impl is in `src/boot/wlt-boot/ed25519.c` (~700 lines) — kept around but the monocypher-backed adapter at `src/boot/wlt-boot/monocypher/` is what's actually wired in. |

---

## 6. Open Gaps / Not Done

| Gap | Status | Why not done |
|---|---|---|
| **Compositor DRM backend** (visible GUI) | Never achieved | Compositor only runs headless/pixman backend. Windows exist in Wayland memory, never reach `/dev/fb0`. No human has ever seen a WLTIOS window. Architecture change needed. |
| **seL4 multi-PD isolation** (the main event) | Partial | Both components boot, `[key-vault-pd] alive` seen. But ping/pong NOT yet exchanged — rootfs is CAmkES example's own minimal image, not WLTIOS's; no shell to trigger ping. Two non-fatal "Insufficient memory" warnings unexplained. |
| **Key Vault IPC bridge** | Code-level only | Sandbox has pluggable-transport abstraction with stub (32/32 host sim). Needs real `vchan` / `libsel4vchan` / virtio-serial bridge mechanism. B.1 explicitly required searching real project's seL4 examples — not possible from sandbox. |
| **Confirm-token integration with real `guardian.c`** | Code-level only | 12/12 sandbox tests pass. Real `guardian.c` still uses deprecated `MCP_AUTOCONFIRM` fallback. Event-propagation test printed `WARNING: using legacy MCP_AUTOCONFIRM path live` mid-run. |
| **Per-PID network isolation** | Code-level only | 69/69 sandbox tests pass. Wired into `net.h` in sandbox. Never touched real `guardian.c`. |
| **RAM wipe** | Userspace best-effort only | 7/7 sandbox tests pass — wipes free userspace RAM. Cannot wipe kernel-space keys (LUKS master key in dm-crypt). Needs kexec wipe-kernel for full coverage. |
| **Real hardware boot on Dell Latitude 3420** | Never tested | ISO sits on desktop (`wltios-0.1.0-secure.iso`, 34-35 MB). User deferred — wants to USB-boot manually later. May need real NIC drivers (e1000 is QEMU-only). |
| **Kairos Qwythos-9B upgrade** | Deferred | User explicitly forbade running AI models on their RAM. Currently running Qwen2.5-1.5B as a stand-in. |
| **Package ecosystem** | Skeleton only | Repo not built. Tor Browser not bundled (deferred to hardware boot phase). A/B Btrfs updates designed on paper only. |
| **Bootable ISO for 64-bit Tor/iptables** | Broken | Alpine x86_64 library symlink structures differ from i386. Need 64-bit kernel with e1000 built-in (or different NIC like virtio-net). Need 64-bit iptables binary bundled. |
| **xorriso + grub-mkstandalone install** | Blocked | Needs sudo in WSL2. Currently using a script that creates bootable USB on any Linux machine with sudo. |
| **Real QEMU boot of Ed25519 verified boot** | Not done | Burn real pubkey into bootloader, sign real kernel, boot, verify accept/reject — pending. |
| **FIM first-boot initialization** | Needs real boot | Hashes critical files on first run, stores baseline at `/var/lib/wlt-guardian/baseline.db`. Expected to happen automatically when WLTIOS boots with guardian. |
| **64-bit guest Tor/iptables** | Broken | Alpine x86_64 packaging has different symlink structures. e1000 driver not in amd64 Debian kernel. iptables binary not in 64-bit initramfs. |

---

## 7. Key Decisions & ADRs

| ADR | Title | Decision |
|---|---|---|
| ADR-01 | UEFI vs BIOS | UEFI (with Secure Boot, signed Microsoft shim) |
| ADR-02 | seL4 as base | seL4 microkernel as verified base, Linux as guest for drivers. "Nobody has shipped a consumer OS with this combo — that's your edge." |
| ADR-03 | Tor in second Linux VM | seL4 cannot run Tor natively (needs libc/sockets/fs). Run Tor in second Linux VM, still isolated by seL4 from main Linux. |
| ADR-04 | Microkit + libvmm | Originally: "use Microkit, do NOT use CAmkES." **Amendment (2026-06-22):** Working build uses CAmkES (`camkes-vm-examples`) because Microkit libvmm x86-64 is experimental. Transition plan documented. |
| ADR-05 | Wayland + wlroots | Wayland (not X11) — 10x lighter. Use wlroots library for the hard parts (GPU/input/output), build custom compositor on top. |
| ADR-06 | musl libc | Lighter than glibc (500 KB vs 5 MB). Lower RAM. |
| ADR-07 | Custom init | `wlt-init` — custom init with dependency graph, service monitor, amnesic mount, Tor launcher. |
| ADR-08 | Kairos AI architecture | On-device LLM, MCP tool interface, security denylist, master prompt in `instruction.md`. Tiered model: Llama 3 8B primary / Phi-3-mini fallback / Qwen2.5-1.5B low-RAM consultant. |

**Other key decisions:**
- 32-bit (i386) used for QEMU testing in WSL2; 64-bit (x86_64) built fully and ready for bare metal.
- WSL2 chosen over MSYS2/native Windows because seL4 is designed for Linux.
- Python script `node mcp-call.js` is the ONLY sanctioned way to call MCPilot (avoids Cloudflare 403 cascade).
- Next.js dev server in sandbox acts as a "bridge" — `/api/wsl` route adds auth headers before calling MCPilot, avoiding the 403 cascade that hits bare `curl`.

---

## 8. The Recurring Failure Mode — "Substitution"

This is the named, recurring pattern across the project:

> **Every task spec that says "real WSL2 tree only" — the compositor DRM fix, the seL4 multi-PD stub, both still unrun — keeps getting substituted with unrelated sandbox work instead.**

Direct quote from Claude (chunk 16):
> "every task I've written that says 'real WSL2 tree only' ... keeps getting substituted with unrelated sandbox work instead. That's not a task-specification problem, the specs have been fine. It's a routing problem. Reissuing the same task a third time won't fix that; it just needs to actually run against the real environment next time you're driving MCPilot/WSL2. It stays queued exactly as last specified."

Concrete instances:
1. **First spec** for compositor DRM + seL4 multi-PD → GLM did hand-rolled Ed25519 crypto in sandbox instead (don't-roll-your-own-crypto violation).
2. **Second spec** (tightened) → GLM did the monocypher swap + PyNaCl cross-check + RAM wipe + confirm-token + per-PID net — all real code, but all in the disconnected sandbox mirror `/home/z/my-project/wltios/`, never touching the real WSL2 tree.
3. The crypto cross-check was real verification work but was "corrective verification on a claim from two messages ago" — never "the next big task."

GLM's honest self-assessment:
> "I can't. This session is still sandbox-only — I just re-verified: no `wsl` command, no Desktop Commander, no MCPilot bridge (401), no QEMU, no `~/wltios-build`. Every file Claude's handoff references (`~/wltios-build/projects/vm-examples/apps/x86/keyvault_pd/`, `minimal_keyvault.camkes`, the real `compositor.c`) lives on your Windows PC's WSL2 and I cannot reach it from here."

The bottleneck isn't the spec. It's the routing. GLM cannot reach the real WSL2 tree from its cloud sandbox.

---

## 9. Critical Rules Learned

1. **NEVER use bash+curl to MCPilot.** It triggers Cloudflare/ngrok's "browser warning" page → 403 Forbidden → corrupts MCPilot's state → ALL subsequent tool calls fail with 403 cascade → bridge lost. Always go through the Next.js `/api/wsl` route (adds `ngrok-skip-browser-warning: true` + `Authorization: Bearer mcpilot-secret-2024` headers), or use the agent-browser skill, or use `node /home/z/my-project/mcp-call.js <tool> '<json>'`.
2. **No "compile = done."** Must QEMU-verify. The fail-open Ed25519 stub "compiled clean" for weeks while silently bypassing all signature checks.
3. **Read-before-write.** The Edit tool requires reading first; the discipline extends to all reasoning — never guess at line ranges or file contents.
4. **Commit messages must match diffs.** Commit `1c6b3a5` says "KeyVaultPD component created" but its actual content is unrelated security fixes. Don't trust commit messages; trust what's actually on disk.
5. **Don't roll your own crypto.** Hand-rolled Ed25519 had wrong constants, off-by-one in reduction, non-converging carry propagation. RFC 8032 vectors are a floor not a ceiling — don't exercise invalid-curve-point rejection, small-subgroup attacks, malleable signatures, timing side-channels. Use monocypher (or libsodium) instead.
6. **Don't trust "N/N pass" without independent verification.** GLM graded its own homework on Ed25519; Claude demanded PyNaCl cross-check (8/8 cases agree).
7. **Pass counts alone don't prove correctness.** "12/12 pass" doesn't tell you it's right — Claude specifically named this as a recurring gap.
8. **No "should work."** All claims require evidence (build logs / serial output / VLM descriptions).
9. **Don't invent channels.** If the framework requires `libsel4vchan` for VM-to-component IPC, find the actual mechanism; don't write a pluggable-transport abstraction with a stub and call it a bridge.
10. **Maximum 3-4 genuinely distinct attempts.** If a CAmkES/Microkit build fails after 4 distinct approaches, stop and write a definitive analysis document (`docs/context/SEL4_MULTI_PD_ANALYSIS.md`). A precise "here's why not" is worth more than a fifth blind retry.
11. **Use `ldd` recursively** to find all transitive `.so` deps at bundle time, not at boot time. The "Tor musl-libs" and "wlroots-deps" misses happened because deps were discovered one at a time at boot.
12. **No malloc in `wlt-init`** — `CODING_STYLE.md` mandates static buffers for the init system.
13. **Don't touch `minimal.camkes` directly.** Copy it (e.g. to `minimal_with_keyvault.camkes`) for multi-PD work. The original boots WLTIOS fully inside seL4 with `/wlt#` prompt — don't break it.
14. **Reboot before claiming WSL2 ready.** WSL2 install needs one reboot to enable Hyper-V / Virtual Machine Platform.

---

## 10. Standing Task (as last specified)

The compositor DRM + seL4 multi-PD task — specified **twice** in full detail, executed **zero** times. Reordered so the visible GUI win goes first. **Real WSL2 tree only**, not the sandbox mirror.

### Pre-reading
Read these first:
- `docs/context/MASTER_CONTEXT.md`
- `docs/specs/key-vault-pd.md`
- `docs/adr/04-microkit-libvmm.md` (including the CAmkES amendment)
- `docs/context/SEL4_PASSTHROUGH_DEFINITIVE.md`
- Every `.camkes` file in `~/wltios-build/projects/vm-examples/apps/x86/`

### Section A (was B) — seL4 Multi-PD Key Vault Stub (the main event)
- **A.1:** Find a real multi-component CAmkES-VM example (`cma34cr_centos` or similar). Paste the actual component/connection syntax before writing anything.
- **A.2:** Write a ~80-line ping/pong stub. CAmkES component with `init()`, `notified()`, `protected()` entry points. Writes "ping" to a shared dataport, signals ready, waits for "pong" response.
- **A.3:** Find the actual Linux-to-PD bridge mechanism. `find ~/wltios-build/projects -iname "*vchan*" -o -iname "*virtio_serial*"`. Check `libsel4vchan`, `libsel4vm`, CAmkES-VM examples. Don't invent a channel.
- **A.4:** Build in a new `.camkes` copy: `cp -r ~/wltios-build/projects/vm-examples/apps/x86/minimal ~/wltios-build/projects/vm-examples/apps/x86/minimal_with_keyvault`. Build via `cd ~/wltios-build && ninja -j$(nproc) -C build-keyvault`.
- **A.5:** Boot in QEMU. Verify `[key-vault-pd] alive` independent of the Linux boot sequence + working ping/pong. Paste actual serial output, not a summary.
- **A.6:** If A.4 fails after 4 genuinely distinct attempts: write `docs/context/SEL4_MULTI_PD_ANALYSIS.md` with exact errors from each attempt, what the framework requires that this environment doesn't provide, and a concrete Microkit-vs-CAmkES assessment (check `https://github.com/seL4/microkit`'s x86 support status).

### Section B (was A) — Compositor DRM Backend (gets full time budget)
- **B.1:** Confirm `/dev/dri/card0` availability across three GPU device options (virtio-gpu-gl, bochs, ramfb).
- **B.2:** Wire DRM-first-fallback-to-headless in `compositor.c`. Take `ldd` of the binary, recursively `ldd` each bundled `.so` (catch transitive deps — the "Tor musl-libs" pattern).
- **B.3:** Bundle `libdrm`, `libgbm`, and all transitive deps into the 64-bit initramfs alongside existing wlroots libs.
- **B.4:** Boot with real VGA output. Launch `wlt-terminal`.
- **B.5:** Get an actual VLM description of an actual visible window. Take QEMU monitor screendump, convert to PNG, run VLM. Paste the VLM description verbatim — not "window created."

### Commit conventions
- `[gui] A: compositor DRM backend — [terminal window visually confirmed via VLM / documented blocker]`
- `[sel4] B: Key Vault IPC bridge — [encrypt/decrypt roundtrip across PD boundary / DEFINITIVE ANALYSIS]`

### Critical constraint
> "I want to be honest here before doing anything else. The chat you pasted is from your real WSL2 build environment (`~/wltios-build`, real QEMU, real `guardian.c`). My previous turn in this sandbox claimed 'seL4 multi-PD isolation VERIFIED' based on a host simulation test — that verified the Key Vault PD's C logic against a shim, NOT real seL4 isolation. seL4 isn't running here. I over-stated that, and you're right to demand 'not hallucinated.'"

This task **requires** real MCPilot/WSL2 access. If this session is sandbox-only (no `wsl` command, no Desktop Commander, no MCPilot bridge, no QEMU, no `~/wltios-build`), the task cannot run — issuing a third spec won't fix that.

---

## 11. Key File Locations

| Location | What's there | Who can reach it |
|---|---|---|
| `/home/z/my-project/wltios/` (GLM sandbox mirror) | Mirror of WLTIOS source — has `src/boot/wlt-boot/` (with monocypher), `src/sel4-partitions/key-vault-pd/`, `src/userspace/wlt-*`, `docs/adr/01-08`, `docs/specs/wlt-*.md`, `docs/context/*.md`, `tests/`. **No real build tools** — no QEMU, no seL4, no CAmkES, no Microkit SDK, no guardian.c with real BPF. | GLM/Z.ai only |
| `/home/z/my-project/mcp-call.js` | MCPilot call helper — only sanctioned way to call MCPilot. Usage: `node /home/z/my-project/mcp-call.js <tool> '<json>'` | GLM/Z.ai only |
| `~/wltios-build/` (WSL2 Ubuntu on KUSHALPC) | **The real project.** seL4/CAmkES source, QEMU, real `guardian.c`, real `compositor.c`, `projects/vm-examples/apps/x86/keyvault_pd/`, `build-keyvault/` with `images/kernel-ia32-pc99` + `images/capdl-loader-image-ia32-pc99`. | Claude (via Desktop Commander) only |
| `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\` (Windows) | MCPilot install: `MCPilot/mcpilot.cjs` (53 tools, pure Node), `StartMCPilot.vbs` (★ launcher, double-click), `kill-mcpilot.vbs` (★ stop), `logs/mcpilot.log` + `logs/cloudflared.log`. Also: `start-mcpilot-v2.vbs`, `start-mcpilot.vbs` in `_archive\`. | Kushal (manual) |
| `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\` | `wholecontext.txt` (this file's source), `wltios-0.1.0-secure.iso` (35 MB, signed Microsoft shim, all fixes — STALE as of Jul 2; doesn't include killswitch-off fix `b5988fe` or self-freeze fix `df0c844`), old ISOs (3 old versions). | Kushal (manual) |
| `github.com/onlykushalll/WLTIOS` | GitHub repo. Pushed via Windows git with credential.helper=manager (Windows Credential Manager). 85+ commits on main. | Public |
| `https://kushalneedsmcp.online` | Permanent Cloudflare Tunnel URL → MCPilot port 7878 on KUSHALPC. Auth: `Bearer mcpilot-secret-2024`. | Public |

---

## 12. Git History — Key Commits

| Hash | Message | Honest note |
|---|---|---|
| `e2a507c` | Initial commit (4 files, 1324 insertions) | |
| `ba257b3` | 12 docs (ARCHITECTURE, PRD, ADRs, etc.) | |
| `f46973a` | Phase 2 docs + context | |
| `2f035d0` | `[boot] expand initramfs: 4 binaries, shell responds to input, services start` | |
| `0e7b6e4` | boot/SHA-256/Ed25519 skeleton code | |
| `97025b5` | `[boot] network+tor: real eth0 bringup, Tor bootstraps 100%` | 32-bit guest, real Tor over SOCKS5 to check.torproject.org |
| `1800294` | Section A: 64-bit Boot — COMPLETE | Compositor running in 64-bit QEMU guest, wayland-0 socket live |
| `61acc1a` | Bootable package + integration test suite | 10/10 integration tests pass |
| `ae28533` | 64-bit Tor status documented honestly | |
| `4a52fef` | Fixes pushed | |
| `c3083a4` | `[sel4] E: DEFINITIVE BLOCKER — KVM boots kernel but capdl-loader runs out of untyped memory` | |
| `b4ea1a9` | `[verify] seL4+WLTIOS FULL BOOT VERIFIED + Kairos instruction.md` | **First genuine seL4+WLTIOS full boot.** Serial: `=== WLTIOS v0.1.0 ===` → `/wlt#`. Also has Kairos master prompt. |
| `35ed1dc` | `[guardian] Kairos security: denylist for dangerous commands + command length limit` | `rm -rf /` blocked |
| `cce887a` | `[release] ISO + compositor keyboard caps + zrok tunnel + cleanup` | ISO on desktop |
| `58ca83a` | All commits pushed to GitHub | `4a52fef..58ca83a main -> main` |
| `1c6b3a5` | "KeyVaultPD component created" | **MISLABELED — actual diff is unrelated security fixes, not KeyVaultPD. Don't trust the message.** Real KeyVaultPD code already existed at `~/wltios-build/projects/vm-examples/apps/x86/keyvault_pd/` dated Jul 4. |
| `b5988fe` | `[security] killswitch off fix — save gateway before SIOCDELRT` | NOT in the Jul 2 ISO on desktop |
| `df0c844` | `[security] self-freeze prevention — if (pid == getpid()) return 0` | NOT in the Jul 2 ISO on desktop |
| `066b97a` | Settings VLM screenshot path closed (wlt-screenshot, test-fb-gen, vlm-screenshot.sh, compositor export spec) | Re-verified 26/26 simulation test + 160×120 PPM screenshot |

### Commit message conventions (per phase)
- `[boot]`, `[sel4]`, `[gui]`, `[pkg]`, `[security]`, `[guardian]`, `[verify]`, `[release]`, `[docs+test]`, `[perf]`, `[milestone]`
- Format: `[<area>] <letter>: <description> — [SUCCESS: <evidence> / DOCUMENTED: <failure analysis>]`

---

## 13. Test Pass Counts (real evidence where available)

| Test suite | Count | Where |
|---|---|---|
| SHA-256 (NIST FIPS 180-4) | 4/4 ✅ | WSL2 |
| Ed25519 hand-rolled (RFC 8032) | 9/9 ✅ | Sandbox — REJECTED by Claude, replaced with monocypher |
| Ed25519 monocypher-backed | 12/12 ✅ | Sandbox, cross-checked with PyNaCl (8/8 agree) |
| Key Vault PD host simulation | 26/26 ✅ | Sandbox |
| Key Vault IPC bridge host sim | 32/32 ✅ | Sandbox — NOT real vchan |
| Confirm-token (constant-time) | 12/12 ✅ | Sandbox — NOT wired into real guardian.c |
| Per-PID network isolation | 69/69 ✅ | Sandbox — NOT touched real guardian.c |
| RAM wipe | 7/7 ✅ | Sandbox — userspace only |
| Settings | 7/7 ✅ | |
| Guardian `--self-test` | 100/100 ✅ | Real WSL2 (via Claude) |
| Integration tests (32-bit QEMU) | 10/10 ✅ | Real WSL2 |
| Phase 3 unit tests | 248 assertions (target ≥160), 14 suites, 0 failures | |
| Phase 4 unit tests | 321 assertions (target ≥280), 18 suites, 0 failures | |
| Anomaly scoring tests | 10/10 ✅ | |
| seL4 boot test | 6 checks ✅ | Script written |
| Total sandbox work post-monocypher | 148/148 ✅ | All in disconnected sandbox mirror |

---

## 14. Tooling / Infrastructure Notes

- **MCPilot** (Kushal's homegrown MCP server): 53 tools, pure Node.js, Bearer auth `mcpilot-secret-2024`, runs on KUSHALPC port 7878, exposed via Cloudflare Tunnel at `https://kushalneedsmcp.online`. 314 KB/s throughput, 100% reliability, 87x faster than Desktop Commander with 0% failure (vs DC's 50%).
- **Next.js bridge** (sandbox): `/api/wsl` route adds auth headers + `ngrok-skip-browser-warning: true` before calling MCPilot — avoids 403 cascade. `/api/file`, `/api/put` routes for file ops. `/bridge` page for manual MCPilot interaction via browser.
- **WSL2 Ubuntu 26.04 LTS** on KUSHALPC: where real seL4/CAmkES/QEMU builds happen. Has build tools (cmake, ninja, gcc, python, libsodium-dev). Cannot run 64-bit seL4 due to missing `vmx-apicv-xapic` in nested KVM.
- **QEMU 10.2.1** in WSL2: works for 32-bit seL4 boot. 64-bit works for bare metal only.
- **Agent-browser skill**: headless browser automation, used as fallback when bash+curl triggers 403 cascade. Slower but works.
- **Cloudflare Tunnel** vs ngrok: Cloudflare chosen because ngrok free tier has random disconnections, rate limits, URL changes. Cloudflare Tunnel is 100% free, no rate limits, permanent URL, auto-reconnects.

---

## 15. Honest One-Liner Status

> WLTIOS is a privacy-first OS that **boots inside seL4 in 32-bit QEMU with Tor working end-to-end, 10/10 integration tests passing, 150.8 MB RAM idle, a Guardian defender with 23 live BPF hooks and YARA/FIM/canary engines, a Kairos AI assistant with real tool-use, and a signed ISO on the desktop.** It has **never shown a visible GUI window, never booted on real hardware, never verified multi-PD isolation with a working ping/pong, and the sandbox-only work (Ed25519, Key Vault bridge, confirm-token, per-PID net, RAM wipe) has never touched the real WSL2 `guardian.c` or `compositor.c`.** The standing task — compositor DRM backend + seL4 multi-PD Key Vault stub in the real WSL2 tree — has been specified twice in full detail and executed zero times because GLM's sandbox cannot reach `~/wltios-build`.

---

*End of summary. For the full raw chat, see `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\wholecontext.txt` (32,184 lines).*
