# WLTIOS Master Context — READ THIS FIRST WHEN CONTEXT RESETS

> **Purpose:** This file captures EVERYTHING important from our conversation.
> If my context fills up and resets, I read this to recover state.
> **Last updated:** 2026-06-21

---

## 1. THE PROJECT: WLTIOS

**Name:** WLTIOS (pronounced "L-TOS")
**Etymology:** **W**indows + **L**inux + **T**ails + **I**nspired **OS**
**Goal:** Build a real desktop operating system in 1-2 years (10-20M lines, AI-assisted)
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

## 7. CURRENT STATE (as of 2026-06-21, 3:30 PM IST)

### What's Done
- ✅ WLTIOS folder at `OneDrive\Desktop\WLTIOS\`
- ✅ All documentation complete:
  - ARCHITECTURE.md (619 lines)
  - PRD.md (501 lines)
  - ROADMAP.md (505 lines)
  - ENGINEERING_FRAMEWORK.md (the constitution, 10 rules)
  - AGENT_ORCHESTRATION.md (7 agent roles)
  - ONBOARDING.md (1-hour guide)
  - CODING_STYLE.md (enforced rules)
  - MODULE_MAP.md (codebase map)
  - CONTRIBUTING.md
  - LICENSE (GPL v3)
- ✅ 7 ADRs (Architecture Decision Records):
  - ADR-01: UEFI vs BIOS → UEFI
  - ADR-02: seL4 as base with Linux guest
  - ADR-03: Tor in second Linux VM (not native seL4 partition)
  - ADR-04: Microkit + libvmm (not CAmkES)
  - ADR-05: Wayland + wlroots (not X11)
  - ADR-06: musl libc (not glibc)
  - ADR-07: Custom init (not systemd)
- ✅ 18 specs (ALL modules spec'd — spec-before-code rule satisfied)
- ✅ 3 research reports (3,813 lines total, 73 cited sources)
- ✅ WLT-Boot skeleton code (6 headers + 6 source files + Makefile)
- ✅ WLT-Init skeleton code (7 headers + 7 source files + Makefile)
- ✅ 5 build scripts (auto-commit, build, test, qemu-test, pre-commit-check)
- ✅ Git initialized, 11 commits pushed to GitHub
- ✅ QEMU 11.0.50 installed on PC (`C:\Program Files\qemu\`)
- ✅ MSYS2 build tools installed (GCC 16.1, CMake 4.3, Ninja 1.13, Python 3.12)
- ✅ seL4 camkes-vm-examples repo cloned (sel4-build/)
- ✅ MCPilot v3.1 running on PC (77 tools)
- ✅ Night shift plan + checklist ready

### What's NOT Done (Tonight's Night Shift)
- ❌ seL4 clean build (needs 3-4 GB RAM — waiting for night shift)
- ❌ QEMU boot test (needs build to succeed first)
- ❌ WLT-Boot implementation (skeleton has TODOs)
- ❌ WLT-Init implementation (skeleton has TODOs)
- ❌ Rust installation (needed for wlt-pkg, wlt-net, etc.)
- ❌ NASM installation (needed for assembly in bootloader)

### Next Steps (Tonight 11 PM)
1. **11 PM:** User closes all apps, restarts MCPilot, says "night shift start"
2. **Verify RAM > 3 GB** (if not, skip build, do code implementation)
3. **seL4 clean build** (the 3-4 GB hurdle)
4. **QEMU boot test** (verify seL4 + Linux works)
5. **WLT-Boot implementation** (fill in TODOs)
6. **WLT-Init implementation** (fill in TODOs)
7. **6 AM:** Final commit, morning report

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

