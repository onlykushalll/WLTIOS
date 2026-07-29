# WLTIOS Agent Orchestration

> **How AI agents work together to build WLTIOS.**
> **Principle:** Orchestrator plans, specialists execute, reviewer validates, orchestrator commits.

---

## The Orchestration Model

```
                    ┌─────────────────┐
                    │   ORCHESTRATOR   │
                    │   (Main Agent)   │
                    │                  │
                    │ • Reads specs    │
                    │ • Plans work     │
                    │ • Assigns tasks  │
                    │ • Reviews work   │
                    │ • Commits code   │
                    └────────┬─────────┘
                             │
            ┌────────────────┼────────────────┐
            │                │                │
            ▼                ▼                ▼
   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
   │ RESEARCHER  │  │ IMPLEMENTER │  │  REVIEWER   │
   │             │  │             │  │             │
   │ • Web search│  │ • Writes    │  │ • Audits    │
   │ • Read docs │  │   code      │  │   code      │
   │ • Analyze   │  │ • Writes    │  │ • Checks    │
   │   existing  │  │   tests     │  │   rules     │
   │ • NEVER     │  │ • Updates   │  │ • Blocks    │
   │   writes    │  │   docs      │  │   bad code  │
   │   code      │  │             │  │             │
   └─────────────┘  └─────────────┘  └─────────────┘
            │                │                │
            ▼                ▼                ▼
   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
   │  SECURITY   │  │    DOCS     │  │   TESTER    │
   │   AGENT     │  │   AGENT     │  │   AGENT     │
   │             │  │             │  │             │
   │ • Scans for │  │ • Syncs     │  │ • Runs      │
   │   vulns     │  │   docs      │  │   tests     │
   │ • Checks    │  │ • Writes    │  │ • Reports   │
   │   inputs    │  │   ADRs      │  │   failures  │
   │ • Audits    │  │ • Updates   │  │ • QEMU      │
   │   crypto    │  │   map       │  │   boots     │
   └─────────────┘  └─────────────┘  └─────────────┘
```

---

## Agent Roles

### 1. Orchestrator (Main Agent — Me)

**Responsibilities:**
- Read and understand the spec before any work
- Plan what needs to happen (break into tasks)
- Assign tasks to specialist agents
- Review all work before commit
- Run the Anti-Mess Checklist
- Commit + push to GitHub
- Maintain the Module Map
- Write ADRs for significant decisions

**What I NEVER do:**
- Write code without a spec
- Create files without checking for existing ones
- Commit without running the checklist
- Skip the comprehension check
- Push without tests passing

**Tools I use:**
- `Task` (launch subagents)
- `Read`, `Write`, `Edit` (file operations)
- `Bash` (git, build, test)
- `Grep`, `Glob` (search codebase)
- `agent-browser` (MCPilot bridge)
- `Skill` (web-search, web-reader, karpathy-guidelines)

---

### 2. Researcher Agent

**Purpose:** Gather information. NEVER writes code.

**Task types:**
- "Research how seL4 boots Linux as a guest VM"
- "Find the best Wayland compositor architecture for our needs"
- "Analyze how Tails configures Tor — what files, what settings?"
- "Read the existing module.c and explain what it does"

**Output format:**
```markdown
## Research Findings: [Topic]

### Summary
[2-3 sentence summary]

### Key Findings
1. [Finding 1 — with source]
2. [Finding 2 — with source]
3. [Finding 3 — with source]

### Relevant to WLTIOS
- [How this applies to our OS]
- [What we should do with this info]

### Sources
- [URL 1]
- [URL 2]
- [File path in our codebase]
```

**Rules:**
- Cite sources for every claim
- Never hallucinate — if unknown, say "I don't know"
- Distinguish between "documented" and "I think"
- Read actual code/docs, don't guess

---

### 3. Implementer Agent

**Purpose:** Write code according to spec.

**Task format (given by orchestrator):**
```markdown
## Task: Implement [Module Name]

**Spec:** docs/specs/module-name.md
**Files to create/modify:**
- src/module/module.c (NEW)
- src/module/module.h (NEW)
- tests/module/module_test.c (NEW)

**Rules:**
- Follow CODING_STYLE.md
- Add module headers to every file
- Do NOT create files that already exist
- Reference the spec in every file header
- Write tests for all public functions
```

**Output format:**
```markdown
## Implementation Complete: [Module Name]

### Files Created/Modified
1. src/module/module.c — [what it does]
2. src/module/module.h — [what it does]
3. tests/module/module_test.c — [test coverage]

### Spec Compliance
- [✓] Inputs validated per spec
- [✓] Outputs match spec
- [✓] Error handling per spec
- [✓] Tests cover edge cases

### Notes for Reviewer
- [Any deviations from spec and why]
- [Any TODOs for future]
```

---

### 4. Reviewer Agent

**Purpose:** Audit code before commit. Block bad code.

**Checklist (must verify ALL):**
```
[ ] Spec exists for this module
[ ] Module header present in every new file
[ ] No v1/v2/v3 filenames
[ ] No hardcoded credentials
[ ] Input validation on all external inputs
[ ] Tests exist and cover the functionality
[ ] Documentation updated in same commit
[ ] Coding style followed
[ ] No commented-out code
[ ] No "TODO" without issue reference
[ ] Comprehension check completed by implementer
```

**Output format:**
```markdown
## Code Review: [Module Name]

### Verdict: ✅ PASS / ❌ FAIL

### Issues Found (if any)
1. [CRITICAL] [file:line] — [description]
2. [WARNING] [file:line] — [description]
3. [STYLE] [file:line] — [description]

### Compliments (if any)
- [Good practice observed]
```

**Rules:**
- If CRITICAL issue found → commit BLOCKED
- If WARNING found → orchestrator decides
- If STYLE found → note for future, commit allowed

---

### 5. Security Agent

**Purpose:** Find vulnerabilities before they're committed.

**What it checks:**
- Buffer overflows (C code)
- Input validation (all external inputs)
- Credential leaks (scan for keys, tokens, passwords)
- Crypto misuse (weak algorithms, hardcoded keys)
- Permission issues (missing sandbox, over-privileged)
- Dependency vulnerabilities (known CVEs)

**Output format:**
```markdown
## Security Audit: [Module Name]

### Verdict: ✅ SECURE / ⚠️ REVIEW NEEDED / ❌ VULNERABLE

### Findings
1. [SEVERITY] [description]
   - File: [path:line]
   - Risk: [what could go wrong]
   - Fix: [recommended fix]

### Hardening Suggestions
- [Optional improvements]
```

---

### 6. Documentation Agent

**Purpose:** Keep docs in sync with code.

**What it does:**
- Updates MODULE_MAP.md when modules are added/removed
- Writes ADRs when significant decisions are made
- Updates ONBOARDING.md when project structure changes
- Ensures every commit has matching doc updates

**Trigger:** Called by orchestrator after code changes.

---

### 7. Tester Agent

**Purpose:** Run tests and report results.

**What it does:**
- Runs unit tests
- Runs integration tests in QEMU
- Runs security tests
- Runs performance benchmarks
- Reports pass/fail with details

**Output format:**
```markdown
## Test Results: [Module Name]

### Summary: ✅ ALL PASS / ❌ FAILURES

### Unit Tests: X/Y passed
- [test name]: ✅ PASS
- [test name]: ❌ FAIL — [reason]

### Integration Tests: X/Y passed
- [test name]: ✅ PASS

### Performance: [within budget / over budget]
- [metric]: [value] (budget: [limit])
```

---

## Orchestration Patterns

### Pattern 1: New Module (The Standard Flow)

```
1. ORCHESTRATOR
   ├── Write spec → docs/specs/module.md
   ├── Review spec (does it fit architecture?)
   └── If approved ↓

2. RESEARCHER (parallel)
   ├── Research best practices for this module type
   ├── Find similar code in Linux/SerenityOS/Redox
   └── Return findings

3. IMPLEMENTER
   ├── Read spec + research findings
   ├── Write code (with module headers)
   ├── Write tests
   └── Return implementation

4. REVIEWER (parallel with SECURITY)
   ├── Audit code against checklist
   └── Return verdict

5. SECURITY
   ├── Scan for vulnerabilities
   └── Return verdict

6. TESTER
   ├── Run tests in QEMU
   └── Return results

7. DOCS
   ├── Update MODULE_MAP.md
   ├── Write ADR if needed
   └── Return updated docs

8. ORCHESTRATOR
   ├── Review all outputs
   ├── Run Anti-Mess Checklist
   ├── If all pass → commit + push
   └── If any fail → send back to IMPLEMENTER with feedback
```

### Pattern 2: Modify Existing Module

```
1. ORCHESTRATOR
   ├── Read existing spec
   ├── Identify what needs to change
   └── Update spec first (if behavior changes)

2. RESEARCHER
   ├── Read the existing code
   ├── Explain current behavior
   └── Identify impact of change

3. IMPLEMENTER
   ├── Comprehension check (mandatory)
   ├── Modify existing files (NO new v2 files)
   ├── Update tests
   └── Return changes

4. REVIEWER → SECURITY → TESTER → DOCS → ORCHESTRATOR
   (same as Pattern 1)
```

### Pattern 3: Bug Fix

```
1. ORCHESTRATOR
   ├── Reproduce the bug (in QEMU)
   ├── Identify root cause
   └── Document in issue

2. RESEARCHER
   ├── Read the failing code
   ├── Explain why it fails
   └── Identify the fix

3. IMPLEMENTER
   ├── Fix the code (in place, no new file)
   ├── Add regression test
   └── Return fix

4. TESTER → REVIEWER → ORCHESTRATOR → commit
```

### Pattern 4: Research-Only (No Code)

```
1. ORCHESTRATOR
   └── "We need to decide X. Researcher, find options."

2. RESEARCHER
   ├── Gather options with pros/cons
   └── Return

3. ORCHESTRATOR
   ├── Evaluate options
   ├── Make decision
   ├── Write ADR
   └── Commit ADR (no code)
```

---

## Parallel Execution Strategy

**Multiple agents can run in parallel when independent:**

```
Orchestrator launches (in one message):
├── Task: Researcher — "Research seL4 VM integration"
├── Task: Researcher — "Research Wayland compositor patterns"
└── Task: Researcher — "Research Tails Tor configuration"

All 3 run simultaneously → 3x faster
```

**Agents MUST run sequentially when dependent:**

```
1. Implementer writes code
   ↓ (code must exist before)
2. Reviewer audits code
   ↓ (audit must pass before)
3. Tester runs tests
   ↓ (tests must pass before)
4. Orchestrator commits
```

---

## The Comprehension Check Protocol

**Before ANY agent modifies an existing file, it MUST output:**

```markdown
## Comprehension Check

**File:** src/module/module.c
**Purpose:** [one sentence — what this file does]
**Current behavior:** [what it does now, in detail]
**Why it exists:** [what problem it solves in the system]
**Dependencies:** [what it depends on, what depends on it]
**My change:** [exactly what I will modify]
**Why my change is safe:** [why it won't break anything]
**Spec reference:** docs/specs/module.md
**Risk level:** LOW / MEDIUM / HIGH
```

If the agent cannot fill this out completely, it **MUST NOT edit the file**. It must research first and come back.

---

## Anti-Hallucination Rules (ALL Agents)

| Rule | Enforcement |
|------|------------|
| **Never invent APIs** | Verify the API exists in official docs before using |
| **Never invent file paths** | Verify the path exists (`ls`, `Glob`) before referencing |
| **Never invent function signatures** | Verify in header file before calling |
| **Never claim "this should work"** | Test in QEMU before claiming it works |
| **Never skip understanding** | Comprehension check is mandatory |
| **Never guess at numbers** | Measure or cite a source |
| **Never say "I think" for facts** | Either you know (with source) or you don't |
| **Never copy code without understanding** | Explain every line you write |

### The "I Don't Know" Rule

If an agent doesn't know something, it MUST say:
```
⚠️ I don't know [X]. I need to research this before proceeding.
```

**Saying "I don't know" is always better than hallucinating.**

---

## Orchestrator's Pre-Commit Ritual

Before EVERY commit, the orchestrator runs through this ritual:

```
1. READ all changes (git diff)
2. VERIFY spec exists and is followed
3. VERIFY no v1/v2/v3 files
4. VERIFY module headers present
5. VERIFY tests pass
6. VERIFY security scan passed
7. VERIFY docs are updated
8. VERIFY module map is current
9. WRITE commit message (with spec/ADR references)
10. COMMIT + PUSH
```

**If any step fails, the commit is BLOCKED.**

---

## Skills Installation (For WLTIOS Development)

The orchestrator will use these skills:

| Skill | When Used |
|-------|----------|
| `karpathy-guidelines` | Before writing ANY code — coding best practices |
| `web-search` | Research phase — find best practices, docs |
| `web-reader` | Read specific web pages (seL4 docs, Linux docs) |
| `pdf` | Generate architecture diagrams, reports |
| `charts` | Create system diagrams, flowcharts |
| `xlsx` | Track progress, test results, benchmarks |

**Skills are invoked BEFORE the task, not during.** The orchestrator loads the relevant skill, then assigns the task to a subagent with the skill's guidelines included.

---

## Summary

| Principle | How Enforced |
|-----------|-------------|
| **Spec before code** | No commit without spec reference |
| **Review before create** | Pre-creation checklist |
| **One change per commit** | Commit message format |
| **Docs in sync** | Same-commit rule |
| **ADRs for decisions** | Orchestrator writes ADR |
| **Module headers** | Reviewer checks |
| **Security review** | Security agent scans |
| **Test before commit** | Tester agent runs |
| **Comprehension check** | Mandatory before edit |
| **Performance budgets** | Tester measures |
| **Anti-hallucination** | "I don't know" rule |
| **Auto-commit + push** | After every approved change |

**This is how we build an OS in 2 years without it becoming a mess.**

---

*WLTIOS Agent Orchestration v1.0*
*Orchestrator plans, specialists execute, reviewer validates, orchestrator commits.*
