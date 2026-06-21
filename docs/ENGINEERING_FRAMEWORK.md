# WLTIOS Engineering Framework

> **The Constitution.** Every agent, every commit, every file must comply.
> **Purpose:** Prevent the 5 vibe-coding failure modes (technical debt, security holes, maintenance nightmares, scale issues, comprehension loss).
> **Status:** ENFORCED — no exceptions.

---

## The 5 Vibe-Coding Traps We Must Avoid

| # | Trap | How WLTIOS Prevents It |
|---|------|----------------------|
| 1 | **House of Cards** (no architecture, inconsistent logic, context collapse) | Spec-Driven Development + Architecture Decision Records + Module Headers |
| 2 | **Security Holes** (hardcoded creds, missing validation, flawed patterns) | Pre-commit security scan + Mandatory input validation + seL4 isolation |
| 3 | **Maintenance Nightmare** (blind troubleshooting, zero docs, comprehension erosion) | Documentation-in-sync rule + ADRs + Onboarding doc |
| 4 | **Scale/Performance** (no load consideration, over-engineering) | Performance budgets + Profiling before optimization |
| 5 | **Skill Atrophy** (pseudo-developer trap) | This framework forces understanding before action |

---

## Rule 1: Spec Before Code (NON-NEGOTIABLE)

**No code is written until a spec exists and is reviewed.**

### What a Spec Contains

Every module/feature must have a spec in `docs/specs/` before implementation:

```markdown
# Spec: [Module Name]

## Purpose
What problem does this solve? (1-2 sentences)

## Inputs
- What comes in? (types, formats, sources)

## Outputs
- What goes out? (types, formats, destinations)

## Dependencies
- What modules/libraries does this depend on?
- What depends on this?

## Error Handling
- What can fail?
- How is failure handled?
- What is logged?

## Tests
- How do we verify this works?
- What are the edge cases?

## Security
- What inputs are untrusted?
- What validation is needed?
- What is the attack surface?

## Performance
- What is the expected throughput?
- What is the latency budget?
- What is the memory budget?
```

### The Flow

```
Idea → Spec → Review → Implement → Test → Review → Commit → Document
         ↑                                                              |
         └─────────────── If change needed, update spec first ──────────┘
```

**Never** write code without a spec. **Never** change code in a way that violates the spec without updating the spec first.

---

## Rule 2: Review Before Create (No v1/v2/v3 Files)

**Before creating ANY new file, the agent MUST:**

1. **Search** for existing files that handle similar responsibility
2. **Verify** no existing file can be modified to serve the purpose
3. **Justify** in the commit message why a new file is needed

### Forbidden Patterns

| Pattern | Why Forbidden | Do Instead |
|---------|--------------|------------|
| `module_v2.c` | Git tracks history — no versioning in filenames | Modify `module.c` in place |
| `module_new.c` | "New" is meaningless | Refactor `module.c` |
| `module_fixed.c` | Fixes go in the original file | Fix `module.c` |
| `module_backup.c` | Git IS your backup | Delete it |
| `temp.c` / `test2.c` | No temp files in production | Use proper names |

### The Pre-Creation Checklist

Before `write_file` for a NEW file, the agent must answer:

```
[ ] I searched for existing files handling this responsibility
[ ] No existing file can be modified to do this
[ ] This file has a single, clear responsibility
[ ] This file's name matches its responsibility
[ ] This file will be referenced in the module map
```

If any box is unchecked, **DO NOT CREATE THE FILE**.

---

## Rule 3: One Change Per Commit

**Each commit does ONE thing. One feature, one fix, one refactor.**

### Commit Message Format

```
[module] action: brief description

- What changed (bullet 1)
- Why it changed (bullet 2)
- Spec reference: docs/specs/module-name.md
- ADR reference (if applicable): docs/adr/NN-decision.md
```

### Examples

✅ Good:
```
[wlt-boot] implement: UEFI memory map parsing

- Added EfiMemoryMap parser to boot/memory.c
- Required for seL4 to know available RAM
- Spec: docs/specs/wlt-boot-memory.md
- ADR: docs/adr/01-uefi-vs-bios.md
```

❌ Bad:
```
updated stuff
fixed bugs
wip
```

### Auto-Commit Workflow

After every work session, the orchestrator runs:

```bash
# 1. Pre-commit checks
./scripts/pre-commit-check.sh

# 2. Stage changes
git add -A

# 3. Commit with message (provided by orchestrator)
git commit -m "$COMMIT_MESSAGE"

# 4. Push to GitHub
git push origin main
```

**This happens automatically after every meaningful change.** No "I'll commit later."

---

## Rule 4: Documentation In Sync

**Code change + documentation change = SAME COMMIT.**

If you:
- Add a function → update the module's README
- Change architecture → update ARCHITECTURE.md
- Add a dependency → update the dependency graph
- Change an interface → update the API docs

### Documentation That Must Always Exist

| Document | Location | Updated When |
|----------|----------|-------------|
| Architecture | `docs/ARCHITECTURE.md` | Architecture changes |
| Module Map | `docs/MODULE_MAP.md` | Module added/removed |
| ADRs | `docs/adr/NN-title.md` | Significant decision made |
| Spec | `docs/specs/module.md` | Before implementing module |
| Onboarding | `docs/ONBOARDING.md` | Project structure changes |
| API Reference | `docs/api/` | Interface changes |

### Module Map (Always Current)

`docs/MODULE_MAP.md` contains a tree of every module, its purpose, and its dependencies. This is the **map of the codebase**. If it's wrong, we're lost.

```
WLTIOS Module Map
├── boot/
│   ├── wlt-boot/          # UEFI bootloader
│   │   ├── memory.c       # Memory map parsing
│   │   ├── loader.c       # seL4 image loader
│   │   └── verify.c       # Signature verification
│   └── ...
├── sel4-partitions/       # seL4 isolated services
│   ├── tor-gateway/       # Tor daemon partition
│   ├── key-vault/         # Encryption key storage
│   └── ...
└── userspace/             # Custom userspace
    ├── wlt-compositor/    # Wayland compositor
    └── ...
```

---

## Rule 5: Architecture Decision Records (ADRs)

**Every significant decision gets an ADR.**

An ADR is a short document that captures:
- **Context:** Why did we need to make a decision?
- **Decision:** What did we decide?
- **Consequences:** What are the implications?

### ADR Format

```markdown
# ADR-NN: [Decision Title]

**Date:** YYYY-MM-DD
**Status:** Proposed | Accepted | Superseded by ADR-XX

## Context
[Why is this decision needed? What problem are we solving?]

## Decision
[What did we decide? Be specific.]

## Consequences
[What are the implications? Positive and negative.]

## Alternatives Considered
[What other options did we reject? Why?]
```

### When to Write an ADR

| Decision | Write ADR? |
|----------|-----------|
| Use seL4 as base | ✅ Yes |
| Use Wayland vs X11 | ✅ Yes |
| Naming a function `foo()` | ❌ No |
| Choose Rust for package manager | ✅ Yes |
| Refactor module structure | ✅ Yes |
| Fix a typo | ❌ No |

**Rule of thumb:** If future-you would ask "why did we do it this way?", write an ADR.

---

## Rule 6: Module Headers (Every File)

**Every source file starts with a header comment.**

### C/C Header

```c
/**
 * @file memory.c
 * @brief Parses UEFI memory map for the bootloader
 * @module wlt-boot/memory
 * @purpose Provides the memory map to seL4 so it knows available RAM
 * @dependencies: efi.h, log.h
 * @tested_by: tests/boot/memory_test.c
 * @spec: docs/specs/wlt-boot-memory.md
 * @adr: docs/adr/01-uefi-vs-bios.md
 */
```

### Rust Header

```rust
//! @file package.rs
//! @brief Package manager core logic
//! @module wlt-pkg/core
//! @purpose Installs, removes, updates packages atomically
//! @dependencies: serde, ring (crypto)
//! @tested_by: tests/package_test.rs
//! @spec: docs/specs/wlt-pkg.md
```

### Why This Matters

When any agent (or human) opens a file, they immediately know:
1. What this file does
2. Why it exists
3. What it depends on
4. How to test it
5. Where the spec is
6. Where the decision rationale is

**No file is a mystery.** This prevents the "comprehension erosion" trap.

---

## Rule 7: Security Review for System Code

**All kernel, seL4, and system code gets security review.**

### Pre-Commit Security Scan

Before every commit, a scan runs:

1. **Hardcoded credentials check** — scan for API keys, passwords, tokens
2. **Input validation check** — all external inputs must be validated
3. **Buffer overflow check** — C code checked for unsafe patterns
4. **Dependency audit** — check for known vulnerable dependencies

### Mandatory Security Practices

| Practice | Where Applied |
|----------|--------------|
| Input validation | ALL external inputs (user, network, file) |
| Output encoding | ALL output to UI/network |
| Least privilege | ALL processes (cgroups + namespaces) |
| Defense in depth | seL4 isolation + app sandbox + firewall |
| No hardcoded secrets | Pre-commit hook enforces |
| Signed packages | Package manager enforces |

---

## Rule 8: Test Before Commit

**Every feature needs a test. Tests run before commit.**

### Test Pyramid

```
         /\
        /  \     Manual (real hardware)
       /----\    QEMU integration tests
      /      \   Unit tests (per module)
     /________\ Static analysis (lint, security scan)
```

### What Must Be Tested

| Component | Test Type | Where |
|-----------|----------|-------|
| Bootloader | QEMU boot test | `tests/boot/` |
| seL4 partitions | Unit + integration | `tests/sel4/` |
| Kernel modules | Unit | `tests/kernel/` |
| Userspace | Unit + integration | `tests/userspace/` |
| Security | Penetration tests | `tests/security/` |
| Performance | Benchmarks | `tests/perf/` |

**No untested code is committed.** If a test fails, the commit is blocked.

---

## Rule 9: Comprehension Check (The Anti-Hallucination Rule)

**Before any agent modifies a file, it MUST demonstrate understanding.**

### The Comprehension Protocol

Before editing `module.c`, the agent must output:

```
COMPREHENSION CHECK:
- File: module.c
- Purpose: [one sentence explaining what this file does]
- Current behavior: [what it does now]
- Why it exists: [what problem it solves in the system]
- My change: [what I will change]
- Why my change is safe: [why it won't break anything]
- Spec reference: [where the spec is]
```

If the agent cannot fill this out, **it does not edit the file.** It must research first.

### Anti-Hallucination Rules

| Rule | Enforcement |
|------|------------|
| Never invent APIs | Verify API exists in docs before using |
| Never invent file paths | Verify path exists before referencing |
| Never invent function signatures | Verify signature in header before calling |
| Never claim "this should work" | Test in QEMU before claiming |
| Never skip understanding | Comprehension check is mandatory |

---

## Rule 10: Performance Budgets

**Every component has a performance budget. Violations are bugs.**

### WLTIOS Performance Budgets

| Component | Budget | Metric |
|-----------|--------|--------|
| Boot time | < 3 seconds | Stopwatch |
| Idle RAM | < 256MB | `free -m` |
| Disk install | < 2GB | `du -sh /` |
| App launch | < 500ms | Stopwatch |
| UI frame rate | 60fps | Frame counter |
| Tor overhead | < 100ms added latency | Network benchmark |
| seL4 IPC | < 1μs | Microbenchmark |

### Profiling Before Optimization

**Never optimize without measuring first.**

```
1. Measure (is it actually slow?)
2. Profile (where is the time going?)
3. Optimize (fix the bottleneck)
4. Re-measure (did it actually help?)
5. Document (what was the improvement?)
```

---

## The Anti-Mess Checklist (Run Before Every Commit)

Before committing, the orchestrator verifies:

```
[ ] Spec exists and is up to date
[ ] No v1/v2/v3 files created
[ ] Module header present in new files
[ ] Module map updated if structure changed
[ ] ADR written if significant decision made
[ ] Documentation updated in same commit
[ ] Security scan passed
[ ] Tests written and passing
[ ] Comprehension check completed
[ ] Performance budget not violated
[ ] Commit message follows format
[ ] No hardcoded credentials
```

If ANY box is unchecked, the commit is **BLOCKED**.

---

## File Structure (Enforced)

```
WLTIOS/
├── docs/                          # All documentation
│   ├── ARCHITECTURE.md           # High-level architecture
│   ├── PRD.md                    # Product requirements
│   ├── ROADMAP.md                # Development roadmap
│   ├── ENGINEERING_FRAMEWORK.md  # This file (the constitution)
│   ├── AGENT_ORCHESTRATION.md    # How AI agents work together
│   ├── CONTRIBUTING.md           # How to contribute
│   ├── ONBOARDING.md             # New dev guide (1-hour read)
│   ├── MODULE_MAP.md             # Codebase map (always current)
│   ├── CODING_STYLE.md           # Naming, formatting rules
│   ├── specs/                    # Module specs (before code)
│   │   ├── wlt-boot.md
│   │   ├── wlt-compositor.md
│   │   └── ...
│   └── adr/                      # Architecture Decision Records
│       ├── 01-uefi-vs-bios.md
│       ├── 02-sel4-as-base.md
│       └── ...
├── src/                           # Source code
│   ├── boot/                     # Bootloader
│   ├── sel4-partitions/          # seL4 isolated services
│   ├── kernel/                   # Linux kernel config + patches
│   ├── userspace/                # Custom userspace
│   │   ├── wlt-compositor/      # Wayland compositor
│   │   ├── wlt-pkg/             # Package manager
│   │   ├── wlt-shell/           # Desktop shell
│   │   └── ...
│   └── tools/                    # Build tools, scripts
├── tests/                         # All tests
│   ├── boot/
│   ├── sel4/
│   ├── kernel/
│   ├── userspace/
│   ├── security/
│   └── performance/
├── scripts/                       # Build + CI scripts
│   ├── build.sh                  # Build the OS
│   ├── test.sh                   # Run all tests
│   ├── pre-commit-check.sh       # Pre-commit security + lint
│   ├── auto-commit.sh            # Auto commit + push
│   └── qemu-test.sh              # Boot test in QEMU
├── build/                         # Build artifacts (gitignored)
├── .gitignore
├── README.md
└── LICENSE
```

---

## Enforcement

This framework is enforced by:

1. **The Orchestrator (me)** — I check every rule before committing
2. **Pre-commit hooks** — Scripts that block bad commits
3. **The Reviewer Agent** — Audits code before it's committed
4. **The Module Map** — If a file isn't in the map, it doesn't exist (officially)

### What Happens When a Rule Is Violated

| Violation | Consequence |
|-----------|-------------|
| Code without spec | Commit blocked, spec must be written first |
| v1/v2/v3 file created | Commit blocked, file renamed |
| Missing module header | Commit blocked, header added |
| Hardcoded credential | Commit blocked, credential removed + rotated |
| Test missing | Commit blocked, test written |
| Doc out of sync | Commit blocked, doc updated |

**No exceptions. No "I'll fix it later." The framework is the law.**

---

## The Promise

By following this framework, WLTIOS will be:

- **Understandable** — Any agent or human can open any file and know what it does
- **Maintainable** — Documentation never goes stale (same-commit rule)
- **Secure** — Security is checked before every commit, not "later"
- **Structured** — No v1/v2/v3 chaos, no orphaned files, no mystery modules
- **Auditable** — Every decision is in an ADR, every change is in git history
- **Comprehensible in 2-3 months** — Because the framework forces understanding

**This is not vibe coding. This is engineered AI development.**

---

*WLTIOS Engineering Framework v1.0 — The Constitution.*
*Every agent, every commit, every file — compliant.*
