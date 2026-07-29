# WLTIOS — Process and Methodology (Consolidated Reference)

> **Supersedes:** `docs/AGENT_ORCHESTRATION.md`, `docs/ENGINEERING_FRAMEWORK.md`,
> `docs/CODING_STYLE.md`, `docs/ONBOARDING.md`, `docs/CONTRIBUTING.md`.
> Archived under `_ARCHIVED_2026-07-29`. `docs/CODE_OF_CONDUCT.md` and
> `docs/CHANGELOG.md` were intentionally left in place, not archived - they
> are ongoing living artifacts (a running log and a community-standards
> doc), not point-in-time status/design docs that this consolidation's
> mandate covers.

---

## 1. Why this document is mostly rules, not status

Everything in this file is process/methodology - it doesn't make factual
claims about what's built, so it doesn't carry the same "verify against
source" burden as the other 9 documents. It's included here because
several of these rules (no-malloc-in-PID-1, no-v1/v2/v3-files,
-Wall-Wextra-Werror) are directly checkable against source and were spot-
confirmed true in passing during this consolidation (e.g. the compositor's
`-Werror` build-health commit, `9c30d8e`).

## 2. Engineering framework - anti-vibe-coding rules

The original framework's stated purpose was preventing exactly the kind of
failure this consolidation task was commissioned to fix: confident false
claims about code that doesn't exist or doesn't work. Its core rules (as
they existed in the archived doc, preserved here):

1. **No claiming "done" without a runnable verification** (build log,
   test output, or QEMU serial output - not "should work").
2. **No modifying v1/v2/v3 copies of a file** - fix the existing file in
   place; git history is the version control, not filename suffixes
   (this rule is explicitly why doc *archiving* in this project uses a
   `_ARCHIVED_<date>` suffix rather than a `v1`/`v2` scheme - the two
   conventions serve different purposes and don't conflict).
3. **Every module file carries a header comment** stating its module,
   purpose, dependencies, and which test file exercises it (confirmed
   still in use - `wlt-sandbox/launch.c`, read directly in this pass,
   has exactly this header format).
4. **No warnings allowed** (`-Wall -Wextra -Werror`) - confirmed as an
   active, enforced constraint, not aspirational (the compositor broke
   this and was specifically fixed, `9c30d8e`, 2026-07-23).

## 3. Agent orchestration

Documents the project's actual workflow: multiple AI systems (referred to
across this project's history as GLM 5.2, Flash 3.5, Opus/Sonnet variants,
Claude) working in different roles - builder, security hardener, bug-finder/
final-hardener - handing off structured work packages (see
`WLTIOS_GUARDIAN_HANDOFF_ARCHIVED_2026-07-29.md` for a concrete, detailed
example of exactly this handoff format in practice). Claude's own role in
this project is planner/instructor, analyst, orchestrator, and code
reviewer, not primary coder - consistent with the perspective this
consolidation itself was performed from (heavy source-reading and
cross-referencing, not authoring new application code).

## 4. Coding style

Key points as documented: Allman brace style, no malloc in PID 1 (WLT-Init
uses static buffers), module header comment on every file, no compiler
warnings tolerated, no `v1`/`v2`/`v3` file variants.

## 5. Contributing / onboarding

The original `ONBOARDING.md` described every phase as "Not started" -
this was accurate on the date it was written (very early in the project)
and is exactly the kind of doc that goes stale fastest and should never be
treated as current without checking its date first. Its structural
content (how to set up the build environment, where specs live, how to add
a new module) is folded into `docs/GLOSSARY_REFERENCE.md` rather than
duplicated here, since that's reference material, not process/methodology
per se.

## 6. How this document set itself should be maintained

Given the demonstrated staleness rate of status claims in this project
(see `docs/STATUS_ROADMAP.md` §1 for the most recent example - a PRD stale
by 6 days against its own repo), any future large doc-consolidation pass
should repeat this pass's method: read the doc, then check `git log` since
its stated date, then spot-check a handful of its most safety-relevant or
surprising claims directly against source before writing a replacement.
