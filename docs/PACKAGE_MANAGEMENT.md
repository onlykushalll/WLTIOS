# WLTIOS — Package Management (Consolidated Reference)

> **Supersedes:** `docs/specs/wlt-pkg.md`. Archived under
> `_ARCHIVED_2026-07-29`.

---

## 1. Design vs. reality

The spec calls for a Nix-style store (`/var/wlt/store/wlt-<hash>-<name>-<version>/`
with symlinks from `/usr/bin/` etc.), Rust implementation, Ed25519-signed
packages downloaded exclusively over Tor, and full atomic
install/remove/update/rollback semantics.

**Reality, per two separate points in the project's history:**

- The **orphaned-code audit** (2026-07-16) found `wlt-pkg` at 827 lines,
  heavily stubbed at that time.
- Substantial, real work landed after that audit: **execve replacing
  system()** for package script execution, dependency-count clamping, and
  atomic database saves (`c11e939`, 2026-07-22); a **real signed package
  repository** with actual Ed25519 sign/verify and tamper rejection,
  0 failures across 2-3 packages (`0eae5b0`/`d796fed`/`1fe4b4a`, 2026-07-21);
  and a further expansion to 3 packages + an index as part of the
  seL4-finish-and-polish workstream (`5dd70af`, 2026-07-25).
- **Implementation language is C, not Rust** - consistent with every other
  module where the original spec called for Rust (see
  `docs/ARCHITECTURE.md` §5).

**Net effect:** treat `wlt-pkg` as materially more complete than the
827-line/heavily-stubbed snapshot the last dedicated audit captured, with a
real signed-repo path now demonstrated end-to-end for a small number of
packages - but the full Nix-style store/symlink/rollback design from the
spec, and a completeness audit at the same rigor as the A/B-update or
orphaned-code audits, does not yet exist. If a rigorous line-by-line
re-audit of `wlt-pkg` is wanted, it should be commissioned specifically -
it wasn't part of any doc read for this consolidation.

## 2. What's confirmed real (via commit inspection, not just message text)

- `execve()`-based script execution (replacing `system()`, which is a
  shell-injection surface) - confirmed as a real, described diff in
  `c11e939`.
- Ed25519 package signing and verification with tamper rejection - the repo
  commits explicitly state "0 failures" across repeated sessions
  (`0eae5b0`, `d796fed`, `1fe4b4a` are three near-identical commit messages
  for what looks like the same work re-landed/re-verified across sessions -
  worth a `git diff` between them if precise history matters later).

## 3. Open items

- No completeness re-audit since 2026-07-16 at the rigor applied to
  `wlt-update` or the orphaned-code batch.
- Full store/symlink/rollback design (per the original spec) not confirmed
  implemented.
- Tor-only download enforcement for the package fetch path not
  independently re-verified in this pass.
- CLI surface (`wltpkg install/remove/update/rollback/...`) completeness
  not re-checked.
