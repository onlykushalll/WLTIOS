# WLTIOS Master Context — READ THIS FIRST WHEN CONTEXT RESETS

> **Purpose:** This file captures EVERYTHING important from our conversation.
> If my context fills up and resets, I read this to recover state.
> **Last updated:** 2026-07-12

---

## 1. THE PROJECT: WLTIOS

**Name:** WLTIOS (pronounced "L-TOS")
**Etymology:** **W**indows + **L**inux + **T**ails + **I**nspired **OS**
**Goal:** Build a real desktop operating system — production-level, all features, outstanding quality
**Scope:** NO line limit. Not 3M, not 10M — whatever it takes. 10M, 20M, 100M lines if needed. Comprehensive, complete, production-ready.
**Timeline:** As fast as possible. No rests. AI-assisted, continuous development.
**Quality bar:** Same or better than Windows/Linux/macOS. Production level. Outstanding.
**GitHub:** https://github.com/onlykushalll/WLTIOS
**PC location:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\WLTIOS\`

### Architecture
```
seL4 microkernel (verified, isolated partitions)
  └── Linux kernel (guest VM, provides hardware drivers)
       └── WLTIOS userspace (custom GUI, system, security, AI layers)
```

### Key Design Decisions
1. **seL4 + Linux hybrid** (not pure from-scratch — reuse Linux drivers)
2. **Tor in seL4 partition** (anonymity isolated from Linux — can't be hacked)
3. **Custom Wayland compositor** (built on wlroots, not X11)
4. **musl libc** (lighter than glibc)
5. **Custom init system** (not systemd — smaller, faster)
6. **Nix-style package manager** (atomic, reproducible, rollback)

### Target Specs
- Boot: < 3 seconds
- Idle RAM: < 256MB
- Disk install: < 2GB
- Custom code: ~3M lines
- Reused code: ~30M lines (Linux kernel)

---

## 2. THE USER

- **Name:** Kushal (GitHub: onlykushalll)
- **Role:** Visionary, reasoner, decision-maker. Does NOT code.
- **Working style:** Wants me to handle ALL coding + automation. He helps with reasoning and can consult other AIs (Claude).
- **What he values:** Honesty, no hallucination, speed, structure, "nothing is impossible"
- **What he hates:** Messy vibe-coded projects, v1/v2/v3 files, AI that doesn't understand its own code
- **PC:** Windows 11, 8GB RAM, 1366x768, i5-1135G7, WiFi (Airtel_Shivangi), Tailscale installed

---

## 3. THE MCPilot BRIDGE (How I Control His PC)

### Architecture
```
AI agent (cloud sandbox or local)
  └── node mcp-call.js <tool> '<json>' (Node fetch, browser User-Agent)
       └── Cloudflare Tunnel (kushalneedsmcp.online)
            └── MCPilot v3.2.0 on Kushal's PC (port 7878)
                 └── 72 tools (file, process, registry, UIA, keyboard, mouse, etc.)
```

### CRITICAL RULE: NEVER use bash/curl/wget to call MCPilot
Cloudflare bot-detects non-browser User-Agents → 403 → corrupts the session.
Always use `node /home/z/my-project/mcp-call.js <tool> '<json-args>'`.

### Key Files
- **Sandbox:** `/home/z/my-project/mcp-call.js` (Node fetch helper with browser UA + auth)
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\mcpilot.cjs`
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\StartMCPilot.vbs` (launcher)
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\kill-mcpilot.vbs` (stopper)

### How to Start MCPilot
Double-click `StartMCPilot.vbs`, or via PowerShell:
```powershell
wscript.exe "C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\kill-mcpilot.vbs"
Start-Sleep 5
wscript.exe "C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\StartMCPilot.vbs"
Start-Sleep 10
curl.exe http://127.0.0.1:7878/health
```

### Auth
- Token: `mcpilot-secret-2024` (env var `MCPILOT_TOKEN`)
- URL: `https://kushalneedsmcp.online` (Cloudflare Tunnel, permanent)
- Port: 7878

### How to Verify It's Up
```bash
node /home/z/my-project/mcp-call.js ping '{}'
# Expected: pong <timestamp>
```

### Common Issues
- **"Too many failed attempts" (rate limiter):** Restart MCPilot (kill + start). The limiter may persist across restarts in v3.2.0 — if it does, edit mcpilot.cjs to exempt loopback IPs, or wait 15 minutes.
- **502 Bad Gateway:** MCPilot (Node) crashed but cloudflared is still running. Usually OOM from QEMU. Kill QEMU (`pkill -f qemu-system`), restart MCPilot.
- **403 Forbidden (Cloudflare):** Someone used curl/bash directly. Restart MCPilot to clear the corrupted session.

---

## 4. THE ENGINEERING FRAMEWORK (Anti-Vibe-Coding Rules)

**Read:** `docs/ENGINEERING_FRAMEWORK.md` (full constitution)
**Read:** `docs/AGENT_ORCHESTRATION.md` (how agents work together)

### The 10 Rules (ENFORCED)
1. **Spec before code** — No code without spec in `docs/specs/`
2. **Review before create** — No new files if existing can be modified. NO v1/v2/v3 files.
3. **One change per commit** — `[module] action: description`
4. **Docs in sync** — Code + doc change = same commit
5. **ADRs for decisions** — Every significant decision documented in `docs/adr/`
6. **Module headers** — Every file has header explaining purpose
7. **Security review** — All system code scanned before commit
8. **Test before commit** — No untested code committed
9. **Comprehension check** — Must understand before editing (mandatory output before edit)
10. **Performance budgets** — Violations are bugs

### Auto-Commit
After every work session, run:
```bash
./scripts/auto-commit.sh "[module] action: description" "Spec: docs/specs/x.md"
```
This auto-commits + pushes to GitHub with security checks.

---

## 5. THE AUDIT PROMPT (From wbm-webaudit.vercel.app)

Saved at: `/home/z/my-project/wltios/docs/context/AUDIT_PROMPT.md`

This is a comprehensive read-only audit framework for web projects. It covers:
- Anti-pattern detection (AI-generated tells)
- 6 scored dimensions (accessibility, performance, security, theming, responsive, anti-patterns)
- 15 usability heuristics
- Legal/compliance flags
- Systemic pattern detection

**Usage:** When auditing any web project, invoke this prompt. It produces `audit.md`.

---

## 6. SKILLS TO LOAD

### For WLTIOS Development
| Skill | When |
|-------|------|
| `karpathy-guidelines` | Before ANY code (loaded ✅) |
| `web-search` | Research phase |
| `web-reader` | Reading seL4/Linux/wlroots docs |
| `charts` | Architecture diagrams |
| `pdf` | Generating design docs |

### For Auditing Web Projects
| Skill | When |
|-------|------|
| `impeccable` | UI/UX audit |
| `taste-skill-v1` | Design taste audit |
| `karpathy-guidelines` | Code quality audit |

---

## 7. CURRENT VERIFIED STATE (as of 2026-07-19)

### Verified (raw evidence, not claims)

| Milestone | Evidence | Date |
|---|---|---|
| seL4 32-bit boot | QEMU serial: /wlt# prompt inside seL4 | 2026-06-21 |
| seL4 + Linux guest | Linux boots as guest VM under seL4 | 2026-06-21 |
| seL4 + Tor together | Tor binary runs + bootstraps inside seL4 | 2026-06-21 |
| seL4 multi-PD isolation | Raw serial log: [key-vault-pd] ping received, pong sent + IRQ 12 count 0->1 + Result from Key Vault: pong + TEST PASSED. Two real PDs communicate bidirectionally through the seL4 kernel boundary. | 2026-07-12 |
| 64-bit boot 9/9 PASS | Raw log: OVERALL: PASS - banner, network UP (10.0.2.15), Tor bootstrapped 100%, compositor + wayland-0, shell prompt, DRM card0 + renderD128, DRM backend, Wayland surface committed | 2026-07-12 |
| Compositor DRM rendering | QEMU screendump: 1280x800, 99.99% non-black pixels. VLM confirms: navy background + status bar with green Tor indicators. First visual confirmation in project history. | 2026-07-12 |
| Verified boot (Ed25519) | monocypher-backed, 12/12 RFC 8032 + attack vectors pass, PyNaCl cross-checked 8/8 agree. Pending: burn real pubkey + sign real kernel + QEMU boot. | 2026-07-11 |
| Guardian 5-layer self-test | 100/100 passing (WSL2 tree) | 2026-07-04 |
| 23 BPF hooks attached | Verified via vmlinux.h transfer | 2026-07-04 |
| Kill switch | Route-based, bidirectionally reversible, audit-logged | 2026-07-04 |
| MAC randomization | Verified | 2026-06-21 |
| Amnesia (tmpfs) | Verified | 2026-06-21 |
| Custom kernel (e1000 built-in) | Linux 5.10.218 built with CONFIG_E1000=y, CONFIG_VIRTIO_NET=y (not modules). No .ko files needed in initramfs. Commit: packages/kernel/wlt-kernel.config + scripts/build-wlt-kernel.sh | 2026-07-19 |
| Real networking (Tor 100%) | QEMU serial: e1000 eth0 detected, wlt-net brings up 10.0.2.15/24 gw 10.0.2.2, Tor bootstraps to 100% (done). Commit: ab054d6 | 2026-07-19 |
| Compositor window management | Multi-window desktop: terminal, file manager, launcher, statusbar, settings, kairos all render simultaneously. XDG shell + z-ordering + resize/minimize/maximize. Commits: 264ad7a, 9df6ae8, b664c44 | 2026-07-13 |
| Kairos AI (in-process) | kairos_window.c calls wlt_ai_query (in-process, no fork+exec). libwlt-ai.a linked. wl_keyboard handling added. AI engine initializes: [kairos] AI engine ready (local LLM). Commit: b5570c5 | 2026-07-19 |
| AI pipeline (code path) | kairos -> wlt_ai_query -> wlt_llm_call_local -> wlt_http_post -> 10.0.2.2:8080. Mock LLM server returns HTTP response. Connection succeeds (not -1). Commit: 60ba39b | 2026-07-19 |
| Permissions enforcement (both branches) | DENY test: grant network=deny -> launch returns -5 (PERMISSION). ALLOW test: grant network=tor-only -> launch returns -7 (cgroup, not perm). Both branches proven. Commit: ab054d6 | 2026-07-19 |
| Guardian FIM fixed | guardian.conf with absolute paths installed. Serial: [fim] initialized (db=/etc/wlt/guardian/fim.db, encrypted=1) [guardian] FIM engine: ready (encrypted=1). Commit: ab054d6 | 2026-07-19 |
| wlt-pkg real install/remove | 9-step test, 0 failures: create package, install (extract+symlink+db), run binary, verify db, is_installed, remove (symlink+store+db cleanup), verify gone. Commit: 3f0f184 | 2026-07-19 |
| Reproducible builds | 35 vendored libs (packages/tor-libs + packages/wl-libs), all Makefiles committed, build-all.sh builds all 4 libraries. Clean rebuild test passed. Commits: 1b06c6e through 3f0f184 | 2026-07-19 |

### Tor fix (honest writeup)

The Alpine (musl) Tor binary segfaulted inside ld-musl-x86_64.so.1 during
startup. The kernel log showed: ld-linux-x86-64[70]: segfault at 0 ip
00007f2f4dda757b ... in ld-musl-x86_64.so.1. Replacing it with Debian's
glibc-compiled Tor (same version, different libc) resolved the crash. Also
added libevent-2.1.so.7 (Debian) which was missing. Three Alpine package-name
fixes were also applied (liblzma->xz-libs, libzstd->zstd-libs,
libcap->libcap2) plus a manual libcap.so.2 symlink. The exact broken
component within the musl dependency set was not isolated - the entire Tor
binary + dependency chain was swapped. The crash is consistent with a
mismatched shared library in the musl set, but the specific .so that caused
the segfault was not identified.

### Compositor DRM fix (honest writeup)

The compositor initially used -vga std -device virtio-gpu-pci (two display
devices). QEMU screendump captured the legacy VGA text console (the primary
display) instead of the virtio-gpu framebuffer. Fixed by using
-device virtio-vga (a single combined VGA+GPU device) with no separate
-vga flag, so screendump captures the virtio-gpu rendered output. The
compositor also needed setenv WLR_RENDERER pixman 1 because QEMU virtio-gpu
does not support EGL/GL contexts - pixman (software rendering) works.

### What is NOT done
- Tor PD (seL4 isolation of Tor in a second Linux guest VM) — ADR-03 decided on "Tor in second Linux VM" approach, spec + ADR being written. Key Vault PD proved the CAmkES multi-PD pattern works; Tor PD needs the same scaffold + IPC proof. This is the current top priority.
- Firewall PD — all network traffic should pass through a seL4 partition (enforces Tor-only, kill switch). Not started.
- Boot Verifier PD — runtime integrity check. Not started.
- AI end-to-end with real LLM — code path proven via mock server, but no real llama-server model loaded in QEMU (QEMU user-mode networking quirk: guest gets 404 via 10.0.2.2 even though mock returns 200 when curled directly from WSL2 — needs debugging).
- Kairos Qwen2.5-1.5B upgrade to Qwythos-9B — still 1.5B stand-in.
- Package repository, Tor Browser, A/B updates — wlt-pkg has real install/remove but no repo/tor browser.
- Real hardware boot on the Dell Latitude — QEMU-only (user dropped hardware boot).
- Guardian sig engine — yara rules not compiled (yarac couldn't run due to libyara.so path without root). .yar copied as .yarc fallback.

### Key file locations (WSL2)
- seL4 multi-PD build: ~/wltios-build/build-keyvault/
- KeyVaultPD source: ~/wltios-build/projects/vm-examples/apps/x86/keyvault_pd/
- 64-bit initramfs: ~/wltios-build/wltios-initramfs64.cpio.gz
- 64-bit kernel: ~/wltios-build/kernel-amd64
- Build script: OneDrive/Desktop/WLTIOS/scripts/build-initramfs64.sh
- Verify script: OneDrive/Desktop/WLTIOS/tests/integration/verify_64bit.sh
- seL4 test script: tests/integration/sel4_multipd_test.sh
- MCPilot workspace: C:\llmworkspace\GLM5.2\ (scripts, logs, screendumps)

---

## 8. IMPORTANT LESSONS LEARNED

### From the Antigravity Disaster
- **NEVER fire keyboard combos blind** (WIN+type+Enter opened 20+ windows)
- **See → Decide → Act → See again.** One action per cycle.
- **Don't rapid-fire automate** when I can't see the screen in real-time
- **Focus-stealing prevention** — Windows blocks background apps from stealing focus. Use `app_control` focus or relaunch the app.

### From the MCPilot Token Bug
- **VBS `set VAR=value &&` adds trailing space** — use `shell.Environment("Process")` instead
- **Dollar signs get eaten by JS template literals** — write PS scripts to .ps1 files instead of inline

### From the Vibe-Coding Research
- **Spec-Driven Development (SDD)** is the antidote to vibe coding
- **ADRs** prevent "why did we do this?" amnesia
- **Module headers** prevent comprehension erosion
- **Pre-commit security scans** prevent credential leaks
- **No v1/v2/v3 files** — Git tracks history, filenames stay clean

---

## 9. KEY PATHS

### Sandbox
- `/home/z/my-project/wltios/` — WLTIOS project (sandbox copy)
- `/home/z/my-project/wltios/docs/` — all documentation
- `/home/z/my-project/mcpilot/dist/mcpilot.cjs` — MCPilot v3.1 source
- `/home/z/my-project/src/app/api/proxy/route.ts` — proxy to MCPilot
- `/home/z/my-project/src/app/bridge/page.tsx` — bridge page
- `/home/z/my-project/dev.log` — dev server log

### PC (via MCPilot)
- `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\WLTIOS\` — main project
- `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\` — MCPilot
- `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\StartMCPilot.vbs` — launcher
- `C:\Program Files\qemu\` — QEMU installed here

---

## 10. THE PROMISE

**WLTIOS will be:**
- Understandable (any agent/human can open any file and know what it does)
- Maintainable (docs never go stale — same-commit rule)
- Secure (security checked before every commit)
- Structured (no v1/v2/v3 chaos)
- Comprehensible in 2-3 months (framework forces understanding)

**This is not vibe coding. This is engineered AI development.**

---

*Read this file first when context resets. Then read ENGINEERING_FRAMEWORK.md and AGENT_ORCHESTRATION.md.*

---

## 11. TIMEZONE + CLOCK

- **User timezone:** Asia/Kolkata (IST, UTC+5:30)
- **How to check current Delhi time:** `TZ="Asia/Kolkata" date +"%Y-%m-%d %H:%M:%S IST (%A)"`
- **System clock is UTC:** `date -u` gives UTC time
- **NEVER assume time of day** — always check with the command above before saying "good morning/night" or planning schedules
- **Night shift hours (IST):** 11 PM - 6 AM (low server load, user asleep)
- **Day hours (IST):** 9 AM - 11 PM (user active, expect interruptions)

