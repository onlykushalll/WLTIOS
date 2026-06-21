# WLTIOS — Complete User Instructions Summary

> **THIS IS THE MOST IMPORTANT FILE.** Read this FIRST when context resets.
> It captures EVERY instruction, preference, and rule the user has ever given.
> **Never lose this. Never overwrite without reading first.**
> **Last updated:** 2026-06-21 9:56 PM IST

---

## 1. THE USER (Kushal)

- **Name:** Kushal
- **GitHub:** onlykushalll
- **Role:** Visionary, reasoner, decision-maker. Does NOT code.
- **What he does:** Helps with reasoning, can consult other AIs (Claude, Gemini), gives direction
- **What I do:** ALL coding, automation, building, testing, committing
- **PC:** Windows 11, 8GB RAM, 1366x768, i5-1135G7, WiFi (Airtel_Shivangi), Tailscale
- **WSL2 password:** wltios2026
- **Timezone:** Asia/Kolkata (IST, UTC+5:30)
- **Check time:** `TZ="Asia/Kolkata" date +"%I:%M %p IST"`

---

## 2. PROJECT SCOPE (CRITICAL — UPDATED)

**User's exact words:** "we are not binding it with 3M lines of code limit — it can be far more greater and much more comprehensive if it needs to - and it will be. so I wouldn't say that wltios will be around 3M lines of code only or estimated limit. it can be any 10-20-100 M .. if it needs to be and if time allows, we will do all fast - with no rests mostly"

### What This Means:
- **NO line limit.** Not 3M, not 10M — whatever it takes
- **NO artificial ceiling.** 10M, 20M, 100M lines if needed
- **Comprehensive, complete, production-ready**
- **As fast as possible. No rests.**
- **Same or better than Windows/Linux/macOS**
- **Outstanding quality**

---

## 3. DEVELOPMENT RULES (From User)

### Anti-Vibe-Coding (User's Explicit Request)
The user listed 5 vibe-coding traps and demanded we fix ALL of them:
1. **Massive technical debt / house of cards** — Must have architecture, consistent logic, no context collapse
2. **Security vulnerabilities** — No hardcoded creds, input validation, no flawed patterns
3. **Maintenance nightmares** — Must have documentation, comprehension, no blind troubleshooting
4. **Scale/performance issues** — Must consider load, no over-engineering
5. **Skill atrophy** — Must understand, not just generate

### Structural Rules (User's Explicit Request)
- **NO v1/v2/v3 files** — "dont just create v1, v2 and so on — review existing ones"
- **NO jumping ahead** — "dont jump ahead or just blindly create new files"
- **Review before create** — Check if file exists BEFORE creating new ones
- **Don't create mess** — "people say vibecoded projects gets messy, and you or even the ai wont understand it after 2-3 months"

### Workflow Rules (User's Explicit Request)
- **Auto-commit + push after every change** — "after everything, auto commit and push changes to github"
- **Use multiple agents** — "use multiple agents workflow, orchestration, intelligently to boost the work dynamically"
- **Install skills** — "install various skills (the best ones for these)"
- **Research, don't hallucinate** — "research, dont hallucinate in any manner, at any time"
- **Don't ask for prompts** — "dont ask me to prompt you everytime"

---

## 4. SAFETY RULES (From User)

### PC Safety (User's Explicit Request)
- **"dont corrupt my system"** — Non-negotiable
- **"dont break or crash my system"** — Non-negotiable
- **"dont close mcpilot accidentally"** — MCPilot is critical infrastructure
- **No second OS on PC** — "i dont wanna do a second boot.. keep it separate purely from my pc"
- **No boot setting changes** — "dont modify my boot settings, right?"
- **No partitioning** — "no second os on my pc. nothing like that"

### What I Can Do (User Approved)
- **"you can test, build without changing my system, you can install too"**
- **"i trust you, but dont break my trust please"**
- **"you can clear ram safely, i am not using anything"** (but DON'T close MCPilot)
- **"dont close chrome - only 1 tab open"** (user may be using it)

### Automation Safety (Learned from Antigravity Disaster)
- **NEVER fire keyboard combos blind** — I opened 20+ windows by blind-typing
- **See → Decide → Act → See again** — One action per cycle
- **Don't rapid-fire automate** when I can't see the screen
- **Use MCPilot tools safely, not blindly**

---

## 5. WORK STYLE (From User)

### Speed
- **"do most of the tasks, do what you can, dont think of it as next phase or next week work"**
- **"if u have time, resources and intelligence, then conquer, complete"**
- **"do safely. after knowing, researching when needed"**

### Night Shifts
- **User will handle night shift logistics** — "i will do something about night shift, you dont worry"
- **I cannot run 6-7 hours straight** — context fills, platform timeouts, 403 cascades
- **Best approach:** User sends "continue" periodically, I work in 1-2 hour chunks

### Research
- **"if u cant find solution after repeated trial n error, deep research. because hitting a wall isnt a solution, finding way is, thinking is"**
- **Don't just try things blindly — understand the problem first**

---

## 6. TECHNICAL DECISIONS (Confirmed with User)

### Architecture
- **seL4 + Linux hybrid** (seL4 as verified base, Linux as guest for drivers)
- **Tor in second Linux VM** (not native seL4 partition — Tor needs libc/sockets/fs)
- **Microkit + libvmm** (not CAmkES — seL4 Foundation recommendation)
- **Wayland + wlroots** (not X11)
- **musl libc** (not glibc)
- **Custom init system** (not systemd)

### Build Environment
- **WSL2 Ubuntu** — approved by user, installed, working
- **QEMU** — for testing (not real hardware boot)
- **32-bit for development** (works in WSL2 QEMU)
- **64-bit for production** (built, ready, needs real hardware or native Linux)

### Why 64-bit Doesn't Boot in WSL2
- WSL2's nested KVM doesn't expose `vmx-apicv-xapic` (APIC Virtualization)
- 64-bit seL4 VMM needs this feature for LAPIC emulation
- 32-bit uses software LAPIC emulation → works
- **99% of code is identical between 32/64-bit**
- **64-bit build is COMPLETE** — just can't boot in WSL2

---

## 7. MCPilot BRIDGE (How I Control Kushal's PC)

### Architecture
```
My sandbox (Next.js on :3000)
  └── /api/proxy (server-side fetch, adds auth header)
       └── ngrok tunnel (vigorous-tricking-thirty.ngrok-free.dev)
            └── MCPilot v3.1 on Kushal's PC (port 7878)
                 └── 77 tools (file, process, registry, UIA, keyboard, mouse, etc.)
```

### Auth
- Token: `mcpilot-secret-2024`
- Ngrok domain: `vigorous-tricking-thirty.ngrok-free.dev` (permanent, reserved)
- Header: `ngrok-skip-browser-warning: true`

### How to Start MCPilot (PowerShell method — most reliable)
```powershell
taskkill /f /im node.exe 2>$null; taskkill /f /im ngrok.exe 2>$null
Start-Sleep 2
$env:MCPILOT_TOKEN = "mcpilot-secret-2024"
Start-Process -FilePath "node" -ArgumentList "C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\mcpilot.cjs","--http","--port","7878" -WindowStyle Hidden
Start-Sleep 3
Start-Process -FilePath "$env:USERPROFILE\ngrok\ngrok.exe" -ArgumentList "http","--domain=vigorous-tricking-thirty.ngrok-free.dev","7878" -WindowStyle Hidden
Start-Sleep 3
```

### Common Issues
- **"Unauthorized" error:** Token mismatch. Restart with `$env:MCPILOT_TOKEN = "mcpilot-secret-2024"` set BEFORE node
- **ngrok 404 HTML:** MCPilot is down. Restart MCPilot.
- **/bridge 404:** Proxy route or bridge page deleted. Recreate from source.

### Key Files
- Sandbox: `/home/z/my-project/src/app/api/proxy/route.ts`
- Sandbox: `/home/z/my-project/src/app/bridge/page.tsx`
- PC: `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\mcpilot.cjs`
- PC: `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\StartMCPilot.vbs`

---

## 8. KEY PATHS

### Sandbox
- `/home/z/my-project/wltios/` — WLTIOS project (sandbox copy)
- `/home/z/my-project/wltios/docs/` — all documentation
- `/home/z/my-project/wltios/docs/context/` — research + recovery files (READ THESE FIRST)
- `/home/z/my-project/wltios/docs/specs/` — 18 module specs
- `/home/z/my-project/wltios/docs/adr/` — 7 Architecture Decision Records
- `/home/z/my-project/wltios/src/` — source code
- `/home/z/my-project/dev.log` — dev server log

### PC (via MCPilot)
- `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\WLTIOS\` — main project
- `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Documents\Utils~\MCPilot\` — MCPilot

### WSL2
- `/home/kushal/wltios-build/` — seL4 source + builds
- `/home/kushal/wltios-build/build-minimal-32/images/` — 32-bit bootable images
- `/home/kushal/wltios-build/build-minimal-64/images/` — 64-bit bootable images

### GitHub
- `https://github.com/onlykushalll/WLTIOS`

---

## 9. CURRENT STATE (as of 2026-06-21 10:00 PM IST)

### Completed
- ✅ All documentation (16,000+ lines)
- ✅ 18 specs, 7 ADRs, 4 research reports
- ✅ WLT-Boot + WLT-Init skeleton code (2,500 lines)
- ✅ 5 build scripts
- ✅ WSL2 Ubuntu installed + working
- ✅ seL4 + Linux built (32-bit + 64-bit)
- ✅ seL4 + Linux boots in QEMU (32-bit, reaches login)
- ✅ 17+ commits on GitHub
- ✅ MCPilot v3.1 running (77 tools)

### In Progress
- ⚠️ Login to Buildroot (timing issue with expect script)
- ⚠️ Running commands inside seL4 guest

### NOT Done
- ❌ 64-bit boot (needs real hardware — build is complete)
- ❌ WLTIOS userspace (not started)
- ❌ Custom bootloader implementation (skeleton only)
- ❌ Custom init implementation (skeleton only)
- ❌ GUI (WLT-Compositor)
- ❌ All other modules

---

## 10. LESSONS LEARNED (Critical)

### From Antigravity Disaster
- NEVER fire keyboard combos blind
- One action per cycle: See → Decide → Act → See again
- Don't automate when you can't see the screen

### From MCPilot Token Bug
- VBS `set VAR=value &&` adds trailing space
- Use `shell.Environment("Process")` in VBS instead
- Dollar signs get eaten by JS template literals — write PS scripts to .ps1 files

### From Vibe-Coding Research
- Spec-Driven Development is the antidote
- ADRs prevent "why did we do this?" amnesia
- Module headers prevent comprehension erosion
- No v1/v2/v3 files — Git tracks history

### From seL4 Build
- seL4 needs Linux (not Windows/MSYS2)
- WSL2 works perfectly for building
- 32-bit uses software LAPIC emulation (works in nested KVM)
- 64-bit needs hardware APIC virtualization (not in WSL2 nested KVM)
- 99% of code is identical between 32/64-bit

---

## 11. THE AUDIT PROMPT

User found a comprehensive audit prompt at https://wbm-webaudit.vercel.app/
Saved at: `docs/context/AUDIT_PROMPT.md`
Use this when auditing any web project. Produces `audit.md`.

---

## 12. USER PREFERENCES (Implicit)

- **Honesty over false confidence** — Say "I don't know" rather than hallucinate
- **Push back when warranted** — Don't just agree; explain tradeoffs
- **Explain technical concepts** — User is learning, wants to understand
- **Don't be overly cautious** — "nothing is impossible" attitude
- **Show progress** — User wants to see things happening
- **Commit frequently** — Auto-commit after every change
- **Don't create mess** — Structured, organized, no chaos
- **Think before acting** — Research first, don't just try things blindly

---

## 13. CONTEXT RESET PROTOCOL

When my context resets, I read these files IN ORDER:

1. **THIS FILE** (`docs/context/INSTRUCTIONS_SUMMARY.md`) — Everything the user ever said
2. `docs/context/MASTER_CONTEXT.md` — Project state + MCPilot details
3. `docs/ENGINEERING_FRAMEWORK.md` — The 10 rules (constitution)
4. `docs/AGENT_ORCHESTRATION.md` — How agents work
5. `docs/MODULE_MAP.md` — Codebase map
6. Check `git log --oneline -10` to see recent work
7. Check MCPilot health: `fetch('/api/proxy?path=/health')`
8. Continue from where I left off

---

*This file is the user's voice when I can't hear them. Never lose it.*
