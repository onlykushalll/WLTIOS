# Night Shift Plan — 2026-06-21

> **Date:** 2026-06-21 (Sunday night)
> **Start:** 11:00 PM IST
> **End:** 6:00 AM IST
> **Duration:** 7 hours
> **Goal:** seL4 build + QEMU boot test + start bootloader code

---

## Prerequisites (User Must Do Before 11 PM)

1. **Close all applications** — Antigravity, Chrome, any heavy apps
2. **Restart MCPilot** — Double-click `StartMCPilot.vbs`
3. **Set PC to never sleep** — Settings → System → Power → Sleep → Never
4. **Leave PC on** — Don't shut down or hibernate
5. **Tell orchestrator:** "night shift start"

---

## Schedule (All times IST)

### 11:00 PM — Startup + Verification
- [ ] Verify MCPilot health (`/api/proxy?path=/health`)
- [ ] Check available RAM (must be > 4 GB for build)
- [ ] If RAM < 3 GB: Skip build, do spec writing instead. Document in log.
- [ ] If RAM ≥ 3 GB: Proceed to build

### 11:15 PM — seL4 Clean Build (The Hurdle)
- [ ] Clean previous build: `rm -rf sel4-build/build-minimal-64`
- [ ] Run: `sh ../init-build.sh -DCAMKES_VM_APP=minimal_64 -DPLATFORM=pc99`
- [ ] Run: `ninja`
- [ ] Monitor build (check every 60s)
- [ ] If build succeeds: Verify images/ directory exists
- [ ] If build fails: Read error log, document, try to fix
- [ ] If build OOMs: Document, move to spec writing (no more builds tonight)

### 12:00 AM — QEMU Boot Test (if build succeeded)
- [ ] Find the built image (images/ directory)
- [ ] Boot in QEMU: `qemu-system-x86_64 -enable-kvm -m 2G -serial mon:stdio ...`
- [ ] Verify seL4 boots (look for seL4 boot messages)
- [ ] Verify Linux guest boots (look for Linux kernel messages)
- [ ] Take screenshot of QEMU window
- [ ] Document results

### 12:30 AM — Commit Build Results
- [ ] Commit build log + QEMU screenshot to GitHub
- [ ] Update MASTER_CONTEXT.md with build status
- [ ] Write ADR if any architectural decisions changed

### 1:00 AM — Start WLT-Boot Code (if build succeeded)
- [ ] Create `src/boot/wlt-boot/` directory
- [ ] Write `include/boot.h` (public interface)
- [ ] Write `boot.c` (main entry point — efi_main)
- [ ] Write `console.c` (UEFI console output)
- [ ] Write `memory.c` (UEFI memory map parsing)
- [ ] Write `loader.c` (seL4 ELF loader)
- [ ] Write `verify.c` (SHA-256 signature verification)
- [ ] Write `config.c` (boot config parsing)
- [ ] Write `linker.ld` (UEFI linker script)
- [ ] Write `Makefile` (build rules)
- [ ] Commit each component as it's completed

### 3:00 AM — Start WLT-Init Code (if WLT-Boot progressing well)
- [ ] Create `src/userspace/wlt-init/` directory
- [ ] Write `include/init.h`
- [ ] Write `init.c` (main entry — PID 1)
- [ ] Write `service.c` (service loading + parsing)
- [ ] Write `monitor.c` (process monitoring)
- [ ] Write `dependency.c` (dependency graph)
- [ ] Write `mount.c` (filesystem mounting)
- [ ] Write `log.c` (logging)
- [ ] Write `ctl.c` (wltctl command)
- [ ] Commit each component

### 5:00 AM — Documentation + Final Commit
- [ ] Update MODULE_MAP.md with new modules
- [ ] Update MASTER_CONTEXT.md with overnight progress
- [ ] Write morning report (what was done, what's next)
- [ ] Final commit + push

### 6:00 AM — Stop
- [ ] Stop all background processes
- [ ] Final git push
- [ ] Write status to MASTER_CONTEXT.md
- [ ] Let PC rest

---

## Fallback Plan (If Build Fails)

If the seL4 build fails or OOMs:

### Alternative 1: Spec Writing (Zero RAM)
- Write specs for remaining Phase 1 modules:
  - WLT-Net (network manager)
  - WLT-Crypto (disk encryption)
  - WLT-Sandbox (app sandboxing)
  - WLT-Terminal (terminal emulator)
  - WLT-FileManager (file manager)
  - WLT-Settings (settings app)

### Alternative 2: Research (Low RAM)
- Research CAmkES vs Microkit build differences
- Research wlroots build on musl libc
- Research Linux kernel minimal config for seL4 guest
- Study tinywl.c source code (for WLT-Compositor implementation)

### Alternative 3: Code Templates (Zero RAM)
- Write skeleton code (headers, Makefiles, empty function stubs)
- Write test skeletons
- Write build scripts

---

## Monitoring + Recovery

### Every 30 Minutes
- Check MCPilot health (via `/api/proxy?path=/health`)
- If MCPilot down: Document, wait for user morning restart, continue with local work
- Check git status (commit anything uncommitted)

### Context Reset Recovery
If my context resets:
1. Read `docs/context/MASTER_CONTEXT.md`
2. Read `docs/ENGINEERING_FRAMEWORK.md`
3. Read `docs/AGENT_ORCHESTRATION.md`
4. Check `git log --oneline -10` to see recent work
5. Continue from where I left off

### If I Get Stuck
1. Try 3 times
2. Document the problem in MASTER_CONTEXT.md
3. Move to next task
4. Leave a note for the user to investigate in the morning

---

## Success Criteria

### Minimum Success (Even if build fails)
- [ ] All Phase 1 specs written
- [ ] All ADRs written
- [ ] CONTRIBUTING.md written
- [ ] MODULE_MAP.md updated
- [ ] At least 3 commits pushed

### Moderate Success (Build works, no code)
- [ ] seL4 builds successfully
- [ ] QEMU boot test done
- [ ] Build results documented + committed
- [ ] All minimum success items

### Full Success (Build + code)
- [ ] seL4 builds successfully
- [ ] QEMU boot test done
- [ ] WLT-Boot skeleton code written (all files, functions stubbed)
- [ ] WLT-Init skeleton code written
- [ ] All committed to GitHub
- [ ] Morning report written

---

## Morning Report Template

```
# Night Shift Report — 2026-06-21/22

## Summary
[Brief summary of what was accomplished]

## Build Status
- seL4 build: [SUCCESS / FAILED / SKIPPED]
- QEMU boot: [SUCCESS / FAILED / SKIPPED]
- If failed: [error summary]

## Commits
[List of commits made overnight]

## What's Done
- [Item 1]
- [Item 2]

## What's Next
- [Item 1]
- [Item 2]

## Issues Encountered
- [Issue 1 + how it was resolved]

## RAM Usage (peak)
- [If measured]
```

---

*This plan is a guide, not a script. Adapt based on what happens during the night.*
