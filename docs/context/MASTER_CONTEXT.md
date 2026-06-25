[Reading 292 lines (total: 292)]

# WLTIOS Master Context — READ THIS FIRST WHEN CONTEXT RESETS

> **Purpose:** This file captures EVERYTHING important from our conversation.
> If my context fills up and resets, I read this to recover state.
> **Last updated:** 2026-06-21

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
My sandbox (Next.js on :3000)
  └── /api/proxy (server-side fetch, adds auth header)
       └── ngrok tunnel (vigorous-tricking-thirty.ngrok-free.dev)
            └── MCPilot v3.1 on Kushal's PC (port 7878)
                 └── 77 tools (file, process, registry, UIA, keyboard, mouse, etc.)
```

### Key Files
- **Sandbox:** `/home/z/my-project/src/app/api/proxy/route.ts` (proxy route)
- **Sandbox:** `/home/z/my-project/src/app/bridge/page.tsx` (bridge page with `window.callMCPilot()`)
- **Sandbox:** `/home/z/my-project/mcpilot/dist/mcpilot.cjs` (MCPilot v3.1 source, 746 lines)
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\mcpilot.cjs`
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\StartMCPilot.vbs` (launcher)
- **PC:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\kill-mcpilot.vbs` (stopper)

### How to Start MCPilot
**PowerShell method (most reliable):**
```powershell
taskkill /f /im node.exe 2>$null; taskkill /f /im ngrok.exe 2>$null
Start-Sleep 2
$env:MCPILOT_TOKEN = "mcpilot-secret-2024"
Start-Process -FilePath "node" -ArgumentList "C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\mcpilot.cjs","--http","--port","7878" -WindowStyle Hidden
Start-Sleep 3
Start-Process -FilePath "$env:USERPROFILE\ngrok\ngrok.exe" -ArgumentList "http","--domain=vigorous-tricking-thirty.ngrok-free.dev","7878" -WindowStyle Hidden
Start-Sleep 3
$h = Invoke-RestMethod "https://vigorous-tricking-thirty.ngrok-free.dev/health" -Headers @{"ngrok-skip-browser-warning"="true"}
Write-Host "Health: $($h | ConvertTo-Json -Compress)"
```

**VBS method:** Double-click `StartMCPilot.vbs` (fixed version uses `shell.Environment("Process")` to avoid trailing-space token bug)

### Auth
- Token: `mcpilot-secret-2024`
- Ngrok domain: `vigorous-tricking-thirty.ngrok-free.dev` (permanent, reserved)
- Header: `ngrok-skip-browser-warning: true` (bypasses ngrok's browser warning page)

### How to Verify It's Up
1. Open `http://localhost:3000/bridge` in agent-browser
2. Run: `agent-browser eval "(async () => { const r = await fetch('/api/proxy?path=/health'); return await r.text(); })()"`
3. Expected: `{"status":"ok","service":"mcpilot","version":"3.1.0","tools":77}`

### Common Issues
- **"Unauthorized" error:** Token mismatch. Kill node, restart with `$env:MCPILOT_TOKEN = "mcpilot-secret-2024"` set BEFORE starting node.
- **ngrok 404 HTML:** MCPilot is down (ngrok is up but nothing on port 7878). Restart MCPilot.
- **/bridge 404:** The proxy route or bridge page got deleted. Recreate from `/home/z/my-project/src/app/api/proxy/route.ts` and `/home/z/my-project/src/app/bridge/page.tsx`.

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

## 7. CURRENT STATE (Phase 5 Verified — 2026-06-25)

### VERIFIED IN QEMU SERIAL OUTPUT (32-bit guest, Debian 5.10.0-30-686):

1. Boot: === WLTIOS v0.1.0 === + seL4 -> Linux -> WLT-Init (v5)
2. Framebuffer: fb0 VESA VGA, 1024x768x24
3. e1000 module loaded: e1000 0000:00:03.0 eth0: (PCI:33MHz:32-bit)
4. MAC randomized: 86:f7:a3:36:30:ca (different each boot)
5. Network UP: 10.0.2.15/24 gw 10.0.2.2
6. Tor: 0.4.9.9, SocksPort 9050, Bootstrapped 15% (15s head-start)
7. netcheck: eth0 carrier=UP, 10.0.2.15 FOUND, routes via eth0
8. Kill switch on: OUTPUT policy DROP, allow 127.0.0.1 + 10.0.2.2
9. TCP test: BLOCKED (Connection refused) — kill switch works
10. torcheck: Connected to Tor SOCKS5, greeting OK (Tor bootstrapping)
11. Sandbox: eth0 No such device — network namespace isolation works
12. Draw: Background filled (navy), Rectangle drawn (cyan 573x368), Text WLTIOS (white 4x scale)
13. Amnesic: all writes in RAM (tmpfs)
14. Screenshot: PNG 1024x768, VLM confirms navy bg + cyan rect + WLTIOS text

### VERIFIED IN WSL2 (64-bit guest, Debian 5.10.0-30-amd64):
1. Compositor: Backend started, Wayland display on wayland-0, XDG shell created
2. Desktop: navy bg + status bar + clock + Tor status rendered to /dev/fb0
3. Draw: real pixel rendering to 1024x768x24 framebuffer, VLM-verified
4. Shell: v1.2 with all commands

### VERIFIED IN WSL2 (Kairos):
1. llama.cpp built, llama-cli + llama-server working
2. Model: Qwen2.5-1.5B-Instruct Q4_K_M (1.1GB)
3. Kairos first words: "Hello! I'm Kairos, the AI assistant built into the WLTIOS operating system."
4. Inference: 50 prompt tokens at 31.67 tok/s, 20 generated at 14.84 tok/s, 2.9s total

### RAM USAGE (32-bit guest):
- MemTotal: 493.2 MB (QEMU allocated 512MB)
- MemAvailable: 342.4 MB
- RAM Used: 150.8 MB (target: <256MB — PASSED)

### TEST ASSERTIONS: 321 pass, 0 fail (18 test suites)

### seL4: BYPASSED (direct kernel boot). Fix documented in SEL4_PASSTHROUGH_DEFINITIVE.md.
### Bootable USB: create-bootable-usb.sh + build/vmlinuz + build/initramfs.gz
### ISO: not built (xorriso needs sudo)

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