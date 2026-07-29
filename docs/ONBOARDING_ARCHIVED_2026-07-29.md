# WLTIOS Onboarding

> **Read this first.** After reading, you should understand the project in 1 hour.
> **Audience:** New contributors (human or AI agent)

---

## 1. What Is WLTIOS? (5 minutes)

WLTIOS (pronounced "L-TOS") is a desktop operating system that combines:

- **Windows-inspired UX** — familiar, flexible, user-friendly
- **Linux-powered foundation** — proven kernel, hardware support
- **Tails-grade anonymity** — Tor everywhere, amnesic, MAC spoofing
- **seL4 verified isolation** — mathematical security for critical services
- **AI-native** — built-in AI assistant at the OS level

**No existing OS combines all five.** That's WLTIOS's unique value.

---

## 2. The Architecture (10 minutes)

Read: [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)

```
┌─────────────────────────────────────────────────────────┐
│                    USER APPLICATIONS                      │
├─────────────────────────────────────────────────────────┤
│              WLTIOS USERSPACE (custom, ~3M lines)         │
│  GUI Layer │ System Layer │ Security Layer │ AI Layer     │
├─────────────────────────────────────────────────────────┤
│              LINUX KERNEL (reuse, ~30M lines)             │
│  Runs as a GUEST on seL4 (paravirtualized)               │
├─────────────────────────────────────────────────────────┤
│              seL4 MICROKERNEL (verified, ~10K lines)      │
│  Isolated partitions: Tor, Keys, Firewall, Boot, Auth    │
├─────────────────────────────────────────────────────────┤
│              BOOTLOADER (custom, ~5K lines)               │
└─────────────────────────────────────────────────────────┘
```

**Key insight:** seL4 runs Linux as a guest. If Linux is hacked, the critical services (Tor, keys, firewall) in seL4 partitions remain secure.

---

## 3. The Rules (10 minutes)

Read: [`docs/ENGINEERING_FRAMEWORK.md`](ENGINEERING_FRAMEWORK.md)

**The 10 rules (all enforced):**

1. **Spec before code** — No code without a spec in `docs/specs/`
2. **Review before create** — No new files if existing can be modified
3. **One change per commit** — `[module] action: description`
4. **Docs in sync** — Code + doc change = same commit
5. **ADRs for decisions** — Every significant decision documented
6. **Module headers** — Every file has a header explaining purpose
7. **Security review** — All system code scanned before commit
8. **Test before commit** — No untested code committed
9. **Comprehension check** — Must understand before editing
10. **Performance budgets** — Violations are bugs

**If you're an AI agent, also read:** [`docs/AGENT_ORCHESTRATION.md`](AGENT_ORCHESTRATION.md)

---

## 4. The Module Map (5 minutes)

Read: [`docs/MODULE_MAP.md`](MODULE_MAP.md) (always current)

This is the map of the entire codebase. Every module, its purpose, its dependencies. **If a file isn't in the map, it doesn't officially exist.**

---

## 5. Project Status (5 minutes)

Read: [`docs/ROADMAP.md`](ROADMAP.md)

Current phase: **Phase 1: Foundation (Months 1-6) — MVP**

| Milestone | Target | Status |
|-----------|--------|--------|
| Bootloader (WLT-Boot) | Month 1 | Not started |
| seL4 + Linux integration | Month 2 | Not started |
| Init system + shell | Month 3 | Not started |
| Terminal + file manager | Month 4 | Not started |
| Basic GUI | Month 5 | Not started |
| MVP (Tor + desktop) | Month 6 | Not started |

---

## 6. How to Contribute (10 minutes)

Read: [`docs/CONTRIBUTING.md`](CONTRIBUTING.md)

### If You're a Human

1. Read the spec for the module you want to work on
2. Write code following [`docs/CODING_STYLE.md`](CODING_STYLE.md)
3. Add module headers to every new file
4. Write tests
5. Update documentation
6. Submit PR

### If You're an AI Agent

1. Read [`docs/AGENT_ORCHESTRATION.md`](AGENT_ORCHESTRATION.md)
2. Complete the Comprehension Check before editing any file
3. Follow the 10 rules in [`docs/ENGINEERING_FRAMEWORK.md`](ENGINEERING_FRAMEWORK.md)
4. Never hallucinate — say "I don't know" if unsure
5. Reference the spec in every file header

---

## 7. Key Files to Know

| File | What It Is |
|------|-----------|
| `docs/ARCHITECTURE.md` | The technical blueprint |
| `docs/PRD.md` | What we're building and why |
| `docs/ROADMAP.md` | When we're building it |
| `docs/ENGINEERING_FRAMEWORK.md` | The rules (constitution) |
| `docs/AGENT_ORCHESTRATION.md` | How AI agents work together |
| `docs/MODULE_MAP.md` | The codebase map |
| `docs/specs/` | Specs for every module (before code) |
| `docs/adr/` | Architecture Decision Records |

---

## 8. Development Environment

### Required Tools

| Tool | Purpose | How to Verify |
|------|---------|--------------|
| QEMU | Test OS in VM | `qemu-system-x86_64 --version` |
| GCC cross-compiler | Compile for x86-64 | `x86_64-elf-gcc --version` |
| Rust | System components | `rustc --version` |
| NASM | Assembler | `nasm --version` |
| Make | Build system | `make --version` |
| Git | Version control | `git --version` |

### Build + Test

```bash
# Build the OS
./scripts/build.sh

# Test in QEMU
./scripts/qemu-test.sh

# Run unit tests
./scripts/test.sh
```

---

## 9. The Promise

By following the Engineering Framework, WLTIOS will be:
- **Understandable** — Any agent or human can open any file and know what it does
- **Maintainable** — Documentation never goes stale
- **Secure** — Security checked before every commit
- **Structured** — No v1/v2/v3 chaos
- **Comprehensible in 2-3 months** — Because we force understanding

---

## 10. Start Here

If you're ready to contribute:

1. **Read** `docs/specs/` to find a module without implementation
2. **Read** the spec for that module
3. **Research** how similar modules work in Linux/SerenityOS/Redox
4. **Write** the code (following all rules)
5. **Test** in QEMU
6. **Commit** with a proper message
7. **Push** to GitHub

**Welcome to WLTIOS. Let's build an OS the right way.**

---

*WLTIOS Onboarding — Read this first, understand in 1 hour.*
