# Contributing to WLTIOS

> **Read this before your first contribution.**
> **Audience:** Human contributors and AI agents

---

## 1. Read These First

Before contributing, read (in order):

1. [`docs/ONBOARDING.md`](ONBOARDING.md) — 1-hour overview
2. [`docs/ENGINEERING_FRAMEWORK.md`](ENGINEERING_FRAMEWORK.md) — The 10 rules (constitution)
3. [`docs/CODING_STYLE.md`](CODING_STYLE.md) — Naming, formatting, security rules
4. [`docs/MODULE_MAP.md`](MODULE_MAP.md) — Where things live
5. The spec for the module you want to work on (`docs/specs/`)

---

## 2. Spec Before Code

**No code without a spec.** This is Rule 1 of the Engineering Framework.

If you want to add a feature:
1. Write a spec in `docs/specs/<module-name>.md`
2. Get the spec reviewed (by a human or the Reviewer agent)
3. Only then write code

If you want to modify an existing module:
1. Read the existing spec
2. Update the spec if behavior changes
3. Then modify the code

---

## 3. No v1/v2/v3 Files

**Never create files with version suffixes.** Git tracks history.

| ❌ Forbidden | ✅ Do Instead |
|-------------|--------------|
| `module_v2.c` | Modify `module.c` in place |
| `module_new.c` | Refactor `module.c` |
| `module_fixed.c` | Fix `module.c` |
| `module_backup.c` | Delete it (Git is your backup) |
| `temp.c` | Use a proper name |

---

## 4. One Change Per Commit

Each commit does ONE thing.

### Commit Message Format

```
[module] action: brief description

- What changed (bullet 1)
- Why it changed (bullet 2)
- Spec reference: docs/specs/module-name.md
- ADR reference (if applicable): docs/adr/NN-decision.md
```

### Actions

| Action | When |
|--------|------|
| `implement` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructure (no behavior change) |
| `docs` | Documentation only |
| `test` | Test additions/changes |
| `chore` | Build, config, tooling |

---

## 5. Module Headers

Every source file starts with a header (see `CODING_STYLE.md` for format).

---

## 6. Tests

Every feature needs a test. No untested code is committed.

```
tests/
├── boot/           # Bootloader tests
├── sel4/           # seL4 partition tests
├── kernel/         # Kernel module tests
├── userspace/      # Userspace tests
├── security/       # Security + penetration tests
└── performance/    # Performance benchmarks
```

---

## 7. Pull Request Process

### For Human Contributors

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Write spec (if new feature)
4. Write code (following CODING_STYLE.md)
5. Write tests
6. Update documentation
7. Commit with proper message format
8. Push to your fork
9. Open Pull Request
10. Address review feedback

### For AI Agents

1. Read the spec
2. Complete the Comprehension Check (see AGENT_ORCHESTRATION.md)
3. Write code (following CODING_STYLE.md)
4. Write tests
5. Update documentation
6. Run pre-commit checks
7. Commit with proper message format
8. Push to GitHub

---

## 8. Code Review Checklist

Before your PR is merged, the reviewer checks:

```
[ ] Spec exists and is followed
[ ] Module header present in every new file
[ ] No v1/v2/v3 filenames
[ ] No hardcoded credentials
[ ] Input validation on all external inputs
[ ] Tests exist and cover the functionality
[ ] Documentation updated in same commit
[ ] Coding style followed
[ ] No commented-out code
[ ] No "TODO" without context
[ ] Comprehension check completed
```

---

## 9. Reporting Bugs

Open a GitHub issue with:

```markdown
## Bug Description
[What happened]

## Expected Behavior
[What should have happened]

## Steps to Reproduce
1. [Step 1]
2. [Step 2]
3. [Step 3]

## Environment
- WLTIOS version: [version]
- Hardware: [CPU, GPU, etc.]
- How did you boot: [USB install / Live USB / QEMU]

## Logs
[Paste relevant logs from /var/log/wlt/]
```

---

## 10. Asking Questions

- **GitHub Issues** — For bugs, feature requests, questions about the codebase
- **Matrix** — For real-time discussion (link TBD)
- **Email** — For security vulnerabilities (wltios-security@protonmail.com — TBD)

---

## 11. License

WLTIOS is licensed under **GPL v3** (to be confirmed). By contributing, you agree your contributions are licensed under the same license.

---

*WLTIOS is built by people who care about doing it right. Thank you for contributing.*
