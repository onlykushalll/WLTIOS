# Night Shift Preflight Checklist

> **Date:** 2026-06-21 (Sunday night)
> **Start:** 11:00 PM IST
> **Run this checklist BEFORE starting any work**

---

## Preflight Checks (Automated)

### 1. MCPilot Health
```bash
# Via bridge:
curl http://localhost:3000/api/proxy?path=/health
# Expected: {"status":"ok","service":"mcpilot","version":"3.1.0","tools":77}
```

### 2. RAM Check (CRITICAL — must be > 3 GB)
```powershell
# On PC:
(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB
# Must return > 3.0 (GB)
# If < 3.0: DO NOT START BUILD. Do spec writing instead.
```

### 3. Build Tools Verified
- [x] GCC 16.1.0 (MSYS2) — `/c/msys64/mingw64/bin/gcc.exe`
- [x] CMake 4.3.4 — `/c/msys64/mingw64/bin/cmake.exe`
- [x] Ninja 1.13.2 — `/c/msys64/mingw64/bin/ninja.exe`
- [x] Python 3.12.13 — `/c/msys64/usr/bin/python.exe`
- [x] QEMU 11.0.50 — `/c/Program Files/qemu/qemu-system-x86_64.exe`
- [x] Git — `C:\Program Files\Git\cmd\git.exe`

### 4. seL4 Source Cloned
- [x] `sel4-build/` directory exists
- [x] `sel4-build/kernel/` exists (seL4 kernel source)
- [x] `sel4-build/projects/` exists (CAmkES, musllibc, etc.)
- [x] `sel4-build/projects/vm-examples/apps/x86/minimal_64/` exists (target)

### 5. Python Dependencies
- [x] python-yaml installed (was missing, now fixed)
- [ ] pyelftools (may need `pacman -S mingw-w64-x86_64-python-pyelftools`)
- [ ] jinja2 (may need `pacman -S mingw-w64-x86_64-python-jinja2`)

### 6. Git State
- [x] Repository at `OneDrive\Desktop\WLTIOS\`
- [x] Remote: `https://github.com/onlykushalll/WLTIOS.git`
- [x] Branch: `main`
- [x] All docs committed + pushed

### 7. Documentation Complete
- [x] ARCHITECTURE.md (619 lines)
- [x] PRD.md (501 lines)
- [x] ROADMAP.md (505 lines)
- [x] ENGINEERING_FRAMEWORK.md (the constitution)
- [x] AGENT_ORCHESTRATION.md (how agents work)
- [x] ONBOARDING.md (1-hour guide)
- [x] CODING_STYLE.md (enforced rules)
- [x] MODULE_MAP.md (codebase map)
- [x] CONTRIBUTING.md
- [x] 7 ADRs (architecture decisions)
- [x] 18 specs (all modules spec'd)
- [x] 3 research reports (seL4, Tails+wlroots, seL4 partitions)

### 8. Skeleton Code Ready
- [x] WLT-Boot: 6 headers + 6 source files + Makefile (skeletons with TODOs)
- [x] WLT-Init: 7 headers + 7 source files + Makefile (skeletons with TODOs)
- [x] 4 build scripts (build.sh, test.sh, qemu-test.sh, pre-commit-check.sh)

---

## Night Shift Tasks (In Priority Order)

### Priority 1: seL4 Build (The Hurdle)
**RAM needed: 3-4 GB**
**Time: 10-20 minutes**
**Risk: OOM crash**

1. Check RAM (must be > 3 GB)
2. Clean build: `rm -rf sel4-build/build-minimal-64`
3. Init build: `sh ../init-build.sh -DCAMKES_VM_APP=minimal_64 -DPLATFORM=pc99`
4. Compile: `ninja`
5. If success: verify `images/` directory
6. If fail: read error, document, try to fix

### Priority 2: QEMU Boot Test
**RAM needed: 1.2 GB**
**Time: 2-5 minutes**

1. Find built image in `images/`
2. Boot in QEMU: `qemu-system-x86_64 -enable-kvm -m 2G -serial mon:stdio ...`
3. Verify seL4 boot messages
4. Verify Linux guest boots
5. Screenshot QEMU window
6. Document results

### Priority 3: Commit Build Results
1. Add build log to repo
2. Commit: `[build] seL4 minimal_64 built successfully`
3. Push to GitHub

### Priority 4: WLT-Boot Implementation
**RAM needed: < 500 MB**
**Time: 2-3 hours**

Fill in the skeleton stubs:
1. `console.c` — UEFI ConOut protocol
2. `memory.c` — GetMemoryMap + parsing
3. `loader.c` — ELF loading
4. `verify.c` — SHA-256 (inline implementation)
5. `config.c` — INI parser
6. `boot.c` — Wire it all together
7. Test compile with `make`
8. Commit after each file

### Priority 5: WLT-Init Implementation
**RAM needed: < 500 MB**
**Time: 2-3 hours**

Fill in the skeleton stubs:
1. `service.c` — INI parser for .service files
2. `dependency.c` — Topological sort
3. `monitor.c` — waitpid loop
4. `tor.c` — IPC stubs (real IPC later)
5. `mount.c` — mount() syscalls
6. `init.c` — already mostly done, wire up
7. Test compile with `make`
8. Commit after each file

### Priority 6: Final Documentation
1. Update MODULE_MAP.md with implementation status
2. Update MASTER_CONTEXT.md with overnight progress
3. Write morning report
4. Final commit + push

---

## Fallback (If Build Fails/OOMs)

If seL4 build fails:
1. Document the error
2. Try to fix (search for error message)
3. If unfixable tonight: skip to Priority 4 (WLT-Boot implementation — zero RAM needed)
4. Leave note in MASTER_CONTEXT.md for morning investigation

---

## Stop Conditions (When to Stop Working)

Stop immediately if:
- MCPilot crashes (can't control PC anymore)
- Platform Bash tool cascade failure (can't use my tools)
- RAM drops below 500 MB (PC might become unresponsive)
- 6:00 AM IST reached (let PC rest)

On stop:
1. Commit everything uncommitted
2. Push to GitHub
3. Write status to MASTER_CONTEXT.md
4. Write morning report

---

*This checklist ensures the night shift is productive and safe.*
