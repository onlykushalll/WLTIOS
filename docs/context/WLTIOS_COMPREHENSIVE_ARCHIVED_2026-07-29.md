# WLTIOS — Comprehensive Project Document

> **Purpose:** Single-source-of-truth document for reconstructing full project
> context in any fresh AI session (ZCode, Claude, GLM, etc.). Read this first.
> **Last updated:** 2026-07-11
> **Maintained by:** Kushal (developer) + AI agents (Claude, GLM/Z.ai)

---

## 1. What WLTIOS Is

**WLTIOS** = a hybrid seL4 + Linux privacy operating system. The pitch:
Tails-level anonymity (Tor-only, amnesia, MAC randomization) but with
seL4's formally-verified partition isolation so that compromising Linux
doesn't compromise Tor or the encryption keys.

**Target hardware:** Dell Latitude 3420 (KUSHALPC), i5-1135G7, 8GB RAM,
Windows 10. Development via WSL2 Ubuntu.

**Core components:**
- **seL4** — the microkernel base (formally verified, isolation guarantees)
- **Linux guest** — runs as a VM under seL4 (the user-facing OS)
- **Tor** — all network traffic routes through Tor, no clearnet fallback
- **Guardian** — 5-layer security engine (YARA, BPF, FIM, anomaly, ransomware)
- **Kairos** — AI assistant (currently 1.5B stand-in, target is Qwythos-9B)
- **WLT-Compositor** — Wayland compositor (wlroots-based)
- **WLT-Shell** — custom desktop (taskbar, launcher, notifications)

---

## 2. People & Roles

| Person/Agent | Role | Environment |
|---|---|---|
| **Kushal** | Developer, user, decision-maker | Windows PC (KUSHALPC) + WSL2 Ubuntu |
| **Claude** | AI agent with WSL2 access via Desktop Commander | Can run `wsl bash`, edit files on PC, build, boot QEMU |
| **GLM / Z.ai (me)** | AI agent in cloud sandbox | `/home/z/my-project/` sandbox; MCPilot bridge to PC when live |
| **MCPilot** | Local MCP server on Kushal's PC | 77 tools, Cloudflare tunnel at `kushalneedsmcp.online` |

---

## 3. Two Environments — Do Not Confuse

| Environment | Location | What's there | What can be verified |
|---|---|---|---|
| **WSL2 build tree** (the real one) | `~/wltios-build/` in WSL2 Ubuntu | seL4/CAmkES, QEMU, guardian.c, compositor.c, real build tools | Real QEMU boots, serial output, self-tests |
| **Sandbox mirror** | `/home/z/my-project/wltios/` | Specs, ADRs, skeleton C (wlt-boot/net/init/shell/terminal/crypto/sandbox), Key Vault PD source | Host-side compile + simulation only. No seL4, no QEMU |

**The recurring failure mode:** "real WSL2 tree only" tasks keep getting
substituted with sandbox-only work. This has happened twice with the
compositor DRM + seL4 multi-PD task. The fix is routing, not spec quality.

---

## 4. MCPilot — The Bridge (Fresh Corrected Context)

> The README.md and SETUP.md on the PC are STALE. This is the truth, from
> the actual `mcpilot.cjs` source (746 lines, verified 2026-07-11).

### Live state
- **Version:** 3.1.0
- **URL:** `https://kushalneedsmcp.online` (Cloudflare Tunnel — NOT zrok)
- **Port:** 7878
- **Auth token:** `mcpilot-secret-2024` (env var `MCPILOT_TOKEN`)
- **Tool count:** **77** (verified two ways: `toolDefs.length` + live `/health`)
- **Tunnel:** `cloudflared.exe tunnel run mcp-tunnel`
- **Live check:** `ping → pong 2026-07-11T10:17:27.057Z`

### How to call MCPilot (CRITICAL — violations corrupt the session)
```
node /home/z/my-project/mcp-call.js <tool> '<json-args>'
```
**NEVER** use curl, wget, or bash to call MCPilot. Cloudflare bot-detects
non-browser User-Agents → 403 → cascades into "broken session" → all
subsequent calls fail until MCPilot restarts. The helper script uses
Node fetch with a real browser User-Agent, which Cloudflare accepts.

### The 77 tools (exact names, snake_case, case-sensitive)
- **File Ops (11):** read_file, read_files, write_file, create_directory, list_directory, move_file, copy_file, delete_file, get_file_info, edit_block, search_files
- **Process (7):** start_process, read_process_output, interact_with_process, force_terminate, list_sessions, list_processes, kill_process
- **System/Desktop (8):** system_info, network_info, clipboard_read, clipboard_write, open_url, open_file, notification, screenshot
- **Git (2):** git_status, git_log (real tools, NOT start_process aliases)
- **Archive (2):** zip_create, zip_extract
- **Network (3):** http_request, download_file, ping
- **Code Analysis (4):** count_lines, find_functions, extract_imports, get_dependencies
- **App/Window/Shell (5):** app_control, window_management, taskbar_control, desktop_icons, shell_execute
- **UI Automation (4):** ui_tree, ui_find, ui_click, ui_read (arg: `window_title`, NOT `windowTitle`)
- **Registry (3):** registry_get, registry_set, registry_delete
- **Services (3):** service_list, service_start, service_stop
- **Env (2):** env_get, env_set
- **Scheduled Tasks (2):** scheduled_task_list, scheduled_task_create
- **Inventory/Diagnostics (11):** installed_apps, battery_info, audio_control, display_info, network_connections, wifi_list, bluetooth_list, process_detailed, disk_usage, startup_items, event_log
- **Raw Shell (1):** powershell_run
- **Data/Encoding (7):** base64_encode, base64_decode, json_format, uuid_generate, regex_test, port_scan, kill_port

### What's stale in the docs (fix pending — see Part 1 of the task prompt)
- **README.md** — heavily stale: says "53/37 tools", dead zrok URL (`mcpilot-kushal.shares.zrok.io`), stop hint references `zrok2.exe`
- **SETUP.md** — partially stale: tool tables sum to ~51, wrong names (`clipboard_get` → `clipboard_read`, `screenshot_capture` → `screenshot`), camelCase args (`windowTitle` → `window_title`), missing 26 tools
- **kill-mcpilot.vbs** — header says "Stop MCPilot + Ngrok" (body is correct, kills cloudflared)
- **Leftover files** — `logs/zrok.log`, `zrok.err`, `ngrok.log`, `overview.txt` reference retired zrok share

### WSL2 access via MCPilot
```bash
node /home/z/my-project/mcp-call.js start_process '{"command":"wsl bash -c \"<command>\""}'
```
This is how Claude found the missing `untyped23_pool` line, fixed two real bugs, and got both seL4 components booting. Long-running builds: use `start_process` (returns PID), then `read_process_output` with that PID.

---

## 5. Current Verified State (honest)

### Boot & kernel
| Item | Status |
|---|---|
| seL4 boots (32-bit, KVM) | ✅ Verified — real `/wlt#` inside seL4 |
| 64-bit direct boot | ✅ Verified — used for GUI/network work |
| seL4 + Tor together | ✅ Verified — Tor bootstraps inside seL4 |
| **Verified boot (Ed25519)** | ✅ Code-level complete — monocypher-backed, 12/12 tests pass, PyNaCl cross-checked 8/8 cases agree. Loader wires `.sig` sidecar. Pending: burn real pubkey, sign real kernel, boot in QEMU |
| **seL4 multi-component isolation** | ⚠️ Both components boot together (Linux + KeyVaultPD), `[key-vault-pd] alive` confirmed. Ping/pong NOT yet verified (boot hit 60s timeout before shell). Two "Insufficient memory" messages undiagnosed but non-fatal |
| Real hardware boot | ⚠️ ISO built + Secure-Boot-signed (Microsoft shim). User said they'd test on Dell — no result reported |

### Network & anonymity
| Item | Status |
|---|---|
| Tor-only networking, no clearnet | ✅ Verified |
| Kill switch (route-based) | ✅ Verified, bidirectionally reversible + audit-logged |
| MAC randomization | ✅ Verified |
| **RAM wipe on shutdown** | ⚠️ Userspace best-effort only (7/7 tests pass). Cannot wipe kernel-space keys (LUKS master key). Needs kexec wipe-kernel for full cold-boot defense |
| **Per-PID network isolation** | ✅ Code-level complete (69/69 tests pass). Never touched real guardian.c |
| Guardian BPF hook enforcing on kill-switch flag | ⚠️ Unverified |

### Security engine (Guardian) — lives in WSL2 tree
| Item | Status |
|---|---|
| 5-layer architecture + self-test | ✅ 100/100 passing (after reset-block fix) |
| YARA detection + enforcement | ✅ Verified |
| FIM tamper detection + auto-restore | ✅ Verified |
| 23 BPF hooks attached | ✅ Verified (vmlinux.h transfer) |
| Self-freeze prevention | ✅ Fixed (`if(pid==getpid()) return 0;`) |
| Ransomware rules (FREEZE→LOG) | ✅ Fixed |
| **Confirm-token for irreversible actions** | ✅ Code-level complete (12/12 tests pass, constant-time verified). Never integrated into real guardian.c — still runs on deprecated `MCP_AUTOCONFIRM` |

### GUI / desktop — the honest problem area
| Item | Status |
|---|---|
| Compositor exposes Wayland globals | ✅ Verified |
| **Compositor renders to visible display** | ❌ Headless-only. Nothing has ever been seen by human or VLM |
| Terminal, filemanager, settings | ⚠️ "Connect and create a window" per serial logs. Never visually confirmed |
| wlt-screenshot utility | ✅ Built + tested (headless framebuffer path) |
| Compositor fb export integration | ⏳ Spec written; patch not applied to real compositor |
| Custom desktop shell (taskbar, launcher, theming) | ❌ Essentially unstarted (skeleton code exists, never run) |

### AI (Kairos)
| Item | Status |
|---|---|
| Basic inference + tool-calling | ✅ Verified (1.5B stand-in model) |
| Qwythos-9B upgrade | ❌ Not done |
| Hotword, screen vision | ❌ Not built |
| General-purpose OS-control MCP | ⚠️ Guardian has security-scoped MCP; broader Kairos MCP discussed, not built |

### Packages
| Item | Status |
|---|---|
| One signed package installs/runs | ✅ Verified |
| Real repository, Tor Browser package, A/B updates | ❌ None |

### Crypto / Key Vault (sandbox-audited)
| Item | Status |
|---|---|
| Key Vault PD crypto (monocypher-backed Ed25519, XSalsa20-Poly1305, HMAC-DRBG) | ✅ Solid — 12/12 RFC 8032 + attack vectors pass, PyNaCl cross-checked |
| Key Vault PD logic (IPC dispatch, keystore, tamper) | ✅ Host-sim 26/26 |
| Key Vault IPC bridge (Unix socket ↔ Microkit PPC) | ✅ Host-sim 32/32. Production needs real vchan transport |
| Real seL4 PD isolation | ⏳ Both boot, ping/pong pending |

---

## 6. Test Suite Summary (all host-sim, code-level)

| Test | Result | What it proves |
|---|---|---|
| Ed25519 (monocypher + attack vectors) | 12/12 ✅ | Crypto is correct, rejects malleable/non-canonical/small-subgroup |
| Ed25519 PyNaCl cross-check | 8/8 ✅ | Independent implementation agrees — not self-graded |
| Key Vault PD | 26/26 ✅ | PD logic correct (crypto, IPC, tamper) |
| Key Vault bridge | 32/32 ✅ | Protocol translation correct |
| confirm-token | 12/12 ✅ | Constant-time, rate-limited, action+target bound |
| per-PID net | 69/69 ✅ | Rule generation + policy logic correct |
| RAM wipe | 7/7 ✅ | Allocates, zeros, frees, idempotent |
| Guardian self-test (WSL2) | 100/100 ✅ | All 5 layers functional |
| **Total** | **148/148 + 8 cross-check** | All code-level tests pass |

---

## 7. Key Bugs Found & Fixed (with file:line)

1. **`verify.c:246-263`** — `wlt_verify_ed25519` was fail-open stub (returned OK unconditionally). Fixed to fail-closed, then replaced with real monocypher call.
2. **`verify.c:231-233`** — `wlt_verify_kernel` did hash-compare, not signature verification. Fixed to delegate to Ed25519 verifier.
3. **`loader.c:412`** — Loader only called `wlt_loader_verify_elf` (format check), never crypto verify. Fixed: now loads `.sig` sidecar and calls `wlt_verify_ed25519`.
4. **`boot.h:15-16`** — Used `bool` without `<stdbool.h>`. Fixed.
5. **`action_engine.c` `g_action_freeze()`** — Self-freeze deadlock. Fixed: `if(pid==getpid()) return 0;`
6. **`action_engine.c` `g_action_kill_switch()`** — Gateway lost on route deletion. Fixed: save gateway to `/run/wlt-killswitch` before delete.
7. **`ransomware_engine.c`** — 3 rules too aggressive (FREEZE on false positives). Fixed: downgraded to LOG.
8. **`guardian.bpf.c`** — 5 cross-segment calls broke compilation. Fixed: replaced with `return 0;` stubs.
9. **`guardian.c` `atoi("true")`** — Returns 0, not 1. Fixed: changed to `1`.
10. **`minimal_keyvault.camkes`** — Missing `vm0.simple_untyped23_pool = 20;` (present in working base, absent in keyvault variant). Fixed by Claude: restored the line.
11. **`wlt-shell/cmds/shell.c`** — Missing `#include <sys/un.h>`. Fixed by Claude.
12. **`scripts/build-initramfs64.sh`** — Missing `-I .` for `xdg-shell-protocol.h`. Fixed by Claude.

### The mislabeled commit
`1c6b3a5` says "KeyVaultPD component created" but the actual diff is unrelated security fixes. This confused the handoff. **Lesson: commit messages must match the actual diff.**

---

## 8. Architectural Decisions (ADRs)

- **ADR-01:** UEFI vs BIOS — chose UEFI (Secure Boot compatibility)
- **ADR-02:** seL4 as base (not Linux-as-host) — the core architectural claim
- **ADR-03:** Tor in a second VM (not the main Linux guest)
- **ADR-04:** Microkit + libvmm (not CAmkES) — seL4 Foundation's recommendation. **De-risking: boot libvmm `simple` example on QEMU x86_64 before committing.**
- **ADR-05:** Wayland + wlroots (not X11)
- **ADR-06:** musl libc (not glibc) — smaller, cleaner
- **ADR-07:** Custom init (not systemd) — `wlt-init`, ~1000 lines
- **ADR-08:** (TBD — check `docs/adr/` in WSL2 tree)

---

## 9. Critical Rules Learned (non-negotiable)

1. **NEVER use bash/curl/wget to call MCPilot** — Cloudflare 403 cascade corrupts the session. Use `node /home/z/my-project/mcp-call.js <tool> '<json-args>'` only.
2. **No "compile = done"** — everything must be QEMU-verified. A passing build proves nothing about runtime behavior.
3. **Read before write** — every real fix came from reading the actual file and comparing against a known-working reference, not guessing at a plausible value.
4. **Commit messages must match the actual diff** — the mislabeled `1c6b3a5` confused the entire handoff.
5. **Don't roll your own crypto** — the hand-rolled Ed25519 was replaced with monocypher (audited, public-domain). RFC test vectors are a floor, not a ceiling.
6. **Verify, don't re-test** — "12/12 pass" is self-grading unless expected values are checked against an independent source (PyNaCl cross-check).
7. **"✅ Done" overstates** — distinguish: verified (real QEMU boot) vs code-level complete (host-sim tests pass) vs not started.
8. **Time-box the boot, not the bundling** — "one more config tweak" is the loop that ate hours. Force the stop decision before sunk-cost kicks in.
9. **Recursive `ldd` is wrong** — `ldd` on the final executable resolves the complete transitive closure. But it can't catch `dlopen`'d drivers (Mesa DRI) — bundle those from `/usr/lib/x86_64-linux-gnu/dri/` separately.
10. **The substitution failure mode** — "real WSL2 tree only" tasks keep getting replaced with sandbox work. The fix is routing (ensure the session has MCPilot bridge access), not re-specifying the task.
11. **Section B (seL4 multi-PD) has no visual dependency** — serial output is the evidence, not screenshots. The VLM gap only affects Section A (compositor DRM).
12. **For visual verification: human eyes first, VLM only if uncertain** — "window vs black frame" is a binary judgment; a human looking at the real PNG is more reliable than a model describing a lossy-relayed copy.
13. **PowerShell quoting** — write a `.ps1` file and execute it with `powershell -File`, don't embed quotes in `start_process` commands.
14. **WSL2 access via MCPilot:** `node mcp-call.js start_process '{"command":"wsl bash -c \"...\""}'`

---

## 10. The Standing Task (compositor DRM + seL4 multi-PD)

> Specified twice in full detail, executed zero times due to the substitution
> routing problem. With MCPilot bridge confirmed live (2026-07-11), this can
> now run for real.

### Section A — Compositor DRM (do first, 90-min cap if stuck)

1. **Probe GPU device:** Boot 64-bit QEMU with `-device virtio-gpu-gl`; from wlt-shell `ls /dev/dri/`. If empty, try `virtio-vga`, then `qxl-vga`. Report which works.
2. **Wire DRM-first/headless-fallback in `compositor.c`:**
   ```c
   struct wlr_backend *backend = NULL;
   if (access("/dev/dri/card0", F_OK) == 0) {
       setenv("WLR_BACKENDS", "drm", 1);
       backend = wlr_backend_autocreate(loop, NULL);
   }
   if (!backend) {
       setenv("WLR_BACKENDS", "headless", 1);
       setenv("WLR_RENDERER", "pixman", 1);
       backend = wlr_headless_backend_create(loop);
       struct wlr_output *output = wlr_headless_add_output(backend, 1366, 768);
       wlr_output_set_name(output, "HEADLESS-1");
   }
   ```
3. **Bundle dependencies:** Run `ldd` on the linked binary. Beyond the linked set, bundle Mesa DRI drivers from `/usr/lib/x86_64-linux-gnu/dri/` (they're `dlopen`'d at runtime, invisible to `ldd`).
4. **Boot with VNC:** `-vga std -display vnc=127.0.0.1:1`. Launch wlt-terminal. Screendump to a PNG file. **Human opens the file directly** — binary judgment "window vs black frame." VLM only if uncertain.
5. **If stuck after 90 min:** write `docs/context/COMPOSITOR_DRM_ANALYSIS.md`, move to Section B.

### Section B — seL4 Multi-PD Key Vault stub (the main event)

**State (from Claude's handoff):**
- `~/wltios-build/projects/vm-examples/apps/x86/keyvault_pd/` exists with `minimal_keyvault.camkes`, `KeyVaultPD.camkes`, `src/key_vault_pd.c`
- Missing `vm0.simple_untyped23_pool = 20;` restored
- Two real bugs fixed (`shell.c` missing include, `build-initramfs64.sh` missing `-I .`)
- Both components boot together — `[key-vault-pd] alive` confirmed on serial
- **NOT done:** ping/pong exchange (boot hit 60s timeout before shell)

**Steps:**
1. **Diagnose two "Insufficient memory" messages** (8MB + 4MB requests). Find the actual call site:
   ```bash
   node /home/z/my-project/mcp-call.js start_process '{"command":"wsl bash -c \"grep -rn 8388608\\|4194304 ~/wltios-build/projects/vm-examples/ 2>/dev/null | head -20\""}'
   ```
   Don't bump a number and hope — find the call site.
2. **Check rootfs contents:**
   ```bash
   node /home/z/my-project/mcp-call.js start_process '{"command":"wsl bash -c \"find ~/wltios-build/build-keyvault -iname rootfs.cpio && mkdir -p /tmp/rfs && cd /tmp/rfs && cpio -id < /path/to/rootfs.cpio && ls -la\""}'
   ```
   If no shell, bundle a minimal static binary that writes "ping" to `kv_src_dp` dataport and signals `kv_ready`.
3. **Read the `.camkes` connection blocks + `key_vault_pd.c` carefully** to understand the exact dataport wire format BEFORE writing the ping client. Match it precisely.
4. **Boot + verify:**
   ```bash
   node /home/z/my-project/mcp-call.js start_process '{"command":"wsl bash -c \"cd ~/wltios-build/build-keyvault && timeout 120 qemu-system-i386 -machine pc -cpu host -m 512M -enable-kvm -kernel images/kernel-ia32-pc99 -initrd images/capdl-loader-image-ia32-pc99 -nographic -append \\\"console=ttyS0 init=/sbin/init\\\" 2>&1 | tee /tmp/multipd-boot.txt\""}'
   ```
   Success bar: `[key-vault-pd] alive` + a ping/pong exchange on serial.
5. **If 4 genuinely distinct attempts fail:** write `docs/context/SEL4_MULTI_PD_ANALYSIS.md` with exact errors, what's missing, whether Microkit beats CAmkES.

### Section C — Tests + docs
Write `tests/integration/` for both. Update `MODULE_MAP.md`, `MASTER_CONTEXT.md`, `ROADMAP.md` — ✅ only where actually verified.

---

## 11. Future Phases (ordered)

### Phase Now (the standing task above)
Compositor DRM + seL4 multi-PD ping/pong. **Closes the two biggest gaps.**

### Phase Next — Visible Desktop
Custom shell for real: taskbar, app launcher, window snapping/management, a real theme (not bitmap-font rendering), keyboard shortcuts beyond Alt+K. This is where "custom UI" starts being true rather than aspirational. **Depends on Phase Now's DRM fix.**

### Phase After — Kairos for real
- Swap in Qwythos-9B (replace 1.5B stand-in)
- Build hotword pipeline
- Screen vision (VLM integration)
- Decide: does Kairos get a general-purpose MCP distinct from Guardian's security-scoped one?

### Phase After — Package ecosystem
- Hosted package repository
- Tor Browser as an installable package
- Verify A/B updates actually run (not just exist as code)

### Phase After — Real hardware push
- Boot on the actual Dell Latitude 3420
- Fix real-hardware-specific issues
- Move from Live-boot to disk install

### Phase After — Confirm-token integration
- Replace `MCP_AUTOCONFIRM` calls in real `guardian.c` with `wlt_confirm_begin`/`check`
- 4-phase migration: shadow → opt-in → default-on → hard removal of legacy path
- Kernel-cmdline `wlt.recovery=1` rollback escape hatch

### Phase After — Full cold-boot defense
- kexec wipe-kernel (the 20% that closes the RAM-wipe gap)
- Tiny dedicated kernel that wipes physical page ranges then powers off
- Crypto code registers keys for kernel-side wiping

### Phase Far — 1.0 hardening
- Security audit (external)
- Tor/Firewall as genuine seL4 PDs (not just Key Vault stub-level)
- Performance polish
- Real Tor Browser fingerprint uniformity (Tails feature gap)

---

## 12. Key File Locations

### Sandbox mirror (`/home/z/my-project/wltios/`)
```
src/boot/wlt-boot/          — bootloader (ed25519.c now monocypher-backed, verify.c, loader.c)
src/sel4-partitions/key-vault-pd/  — Key Vault PD (keyvault.c, crypto.c, keystore.c, tests)
src/userspace/wlt-crypto/   — kv_bridge.c (Unix socket ↔ PPC translator)
src/userspace/wlt-confirm/  — confirm-token library
src/userspace/wlt-net/      — kill_switch.c, pid_net.c (per-PID isolation)
src/userspace/wlt-init/     — ram_wipe.c, shutdown.c
src/userspace/wlt-screenshot/ — wlt-screenshot.c (headless framebuffer capture)
docs/specs/                 — all specs (wlt-ram-wipe.md, wlt-confirm.md, wlt-pid-net.md, etc.)
docs/context/               — SECURITY_AUDIT_REAL_FINDINGS.md, ROADMAP_HONEST_STATE.md,
                              ED25519_CROSSCHECK.md, ED25519_MONOCYPHER_SWAP.md, etc.
docs/adr/                   — 01-uefi-vs-bios.md through 07-custom-init.md
tests/                      — boot/ed25519_test.c, userspace/ram_wipe_test.c, integration/
```

### WSL2 build tree (`~/wltios-build/` — reachable via MCPilot)
```
projects/vm-examples/apps/x86/keyvault_pd/  — the real Key Vault PD (Claude's work)
  minimal_keyvault.camkes  — system description (fixed: untyped23_pool restored)
  components/KeyVaultPD/   — CAmkES component definition
  src/key_vault_pd.c       — ping/pong stub (ready_wait → "ping" → "pong" → done_emit)
build-keyvault/            — build directory (ninja here after .camkes changes)
images/kernel-ia32-pc99   — seL4 kernel
images/capdl-loader-image-ia32-pc99  — capdl loader
```
Guardian, compositor, and real wlt-* source also live in this tree (not in sandbox).

### Windows PC (`C:\Users\Default.L-HCG-9FVVGS3\`)
```
OneDrive\Documents\Utils~\
  StartMCPilot.vbs          — launcher (current, Cloudflare tunnel)
  kill-mcpilot.vbs          — stopper (header stale, body correct)
  MCPilot\
    mcpilot.cjs             — THE server source (746 lines, 77 tools, v3.1.0)
    README.md               — STALE (zrok URL, wrong tool count)
    SETUP.md                — PARTIALLY STALE (wrong tool names, missing 26 tools)
    logs\                   — mcpilot.log, cloudflared.log (current);
                              zrok.log/err, ngrok.log, overview.txt (stale, delete)
    _archive\               — v1/v2/v3 source archives (keep)
OneDrive\Desktop\
  wholecontext.txt          — the full 32,184-line chat history (summarized in
                              /home/z/my-project/PROJECT_CONTEXT_SUMMARY.md)
```

---

## 13. Git History (key commits)

### Sandbox mirror (`/home/z/my-project/wltios/`)
```
f553647 [tails-gap] RAM wipe on shutdown (cold-boot defense) + wire pid_net into net.h
3372bb0 [crypto] replace hand-rolled Ed25519 with monocypher (audit concern addressed)
a2e64c0 [security] close remaining gaps: hash-verify→Ed25519, confirm-token, per-PID net
400604c [verified-boot+kvbridge] wire Ed25519 verifier into loader, sign tool, tests, docs
05de629 [security] honest audit: fix fail-open Ed25519 stub + document real gaps
066b97a [phase9] seL4 multi-PD isolation: Key Vault PD verified (26/26) + Settings VLM screenshot path
```

### WSL2 tree (`~/wltios-build/`)
```
df0c844 [security] killswitch off FIXED: saves gateway before route deletion
b5988fe [security] socket hang fixed: self-freeze + ransomware rules downgraded
5c2a582 [security] BPF tracepoints ATTACHED: 23 hooks live + tracefs mount
1c6b3a5 [MISLABELED] says "KeyVaultPD component created" but diff is unrelated security fixes
```

---

## 14. The Original-Pitch Gap (worth naming)

The user's original pitch led with **"Windows-inspired UX"** as feature #1.
What happened: enormous depth on seL4/security/networking; almost nothing
on the desktop experience. The backend had to exist first — but "custom UI"
has drifted from requirement #1 to the least-developed thread. Future phases
should deliberately re-center it, not let it keep drifting by default.

---

## 15. Tails Feature Mapping

### Tails features WLTIOS genuinely has (verified)
- Tor-only networking, no clearnet fallback ✅
- Kill switch (network dies if Tor dies) ✅
- Amnesia (tmpfs-only, nothing survives reboot) ✅
- MAC randomization per boot ✅
- Boot from removable media (ISO/USB-Live) ✅

### Tails features NOT yet ported
- **RAM wipe on shutdown** — ⚠️ userspace best-effort only (needs kexec for kernel-space keys)
- **Uniform Tor Browser fingerprint** — no Tor Browser package exists yet
- **Per-app network isolation** — ✅ code-level only, never touched real guardian.c
- **The architectural advantage** — ❌ seL4 partition isolation not yet real (everything runs in one undivided Linux VM). This is the whole point of WLTIOS over Tails, and it's the standing task.

---

## 16. How to Use This Document

1. **Starting a fresh session:** Read this document first. It reconstructs full context without the 32K-line chat bloat.
2. **The two companion files:**
   - `/home/z/my-project/PROJECT_CONTEXT_SUMMARY.md` — 419-line deep summary of the chat history
   - `/home/z/my-project/MCPILOT_FRESH_CONTEXT.md` — 276-line MCPilot audit (the source of truth for the bridge, not the stale docs)
3. **Before any task:** Check §9 (Critical Rules) and §3 (Two Environments — don't confuse sandbox with WSL2).
4. **The MCPilot helper:** `/home/z/my-project/mcp-call.js` — use it for all bridge calls. Never curl/bash.
5. **The standing task:** §10 — compositor DRM + seL4 multi-PD. Real WSL2 tree only. MCPilot bridge confirmed live 2026-07-11.

---

*This document is the single source of truth. If anything in it conflicts
with a stale doc on the PC, this document wins (the PC docs are known stale
and pending fix). Last verified: 2026-07-11T11:22Z, MCPilot `ping → pong`.*
