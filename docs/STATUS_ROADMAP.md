# WLTIOS — Project Status and Roadmap (Consolidated Reference)

> **Supersedes:** `docs/ROADMAP.md`, `docs/PRD.md`,
> `docs/context/WLTIOS_COMPLETE_ROADMAP.md`,
> `docs/context/WLTIOS_COMPREHENSIVE.md`,
> `docs/context/PROJECT_CONTEXT_SUMMARY.md`,
> `docs/context/PROGRESS_LOG.md`, `docs/context/PHASE_SEL4_FINISH_AND_POLISH.md`,
> `docs/context/PRD_V1_ARCHIVED_2026-06-20.md`,
> `docs/context/ROADMAP_V1_ARCHIVED_2026-06-20.md`,
> `PHASE5_FINAL_STATUS.md`. Archived under `_ARCHIVED_2026-07-29`.
> **This document exists because this project has a documented history of
> confidently false status claims** (a roadmap once called ~7,000 real
> lines of code "not started"; a commit once claimed a feature was wired in
> when the committed file had zero trace of it; `wlt-update` was called
> "skeleton" when it was 2,088 lines with zero stubs). Every status line
> below is dated and sourced to either a direct source-code read, a
> `git show` diff, or an explicit note about what was *not* re-checked.

---

## 1. The most important finding of this consolidation

**The project's own most-recent PRD (2026-07-22/23) is itself already
6-plus days stale relative to `git log`.** Between 2026-07-23 and
2026-07-28, real, verifiable work landed that the PRD's "Not started" /
"in progress" lists do not reflect:

| PRD said | Git history (2026-07-24 through 07-28) shows |
|---|---|
| Boot Verifier PD: not started | Scaffold (`eddb02d`) + real SHA-256 comparison (`16ef5b9`) |
| Verified boot: stub/in-progress | Real Ed25519 via monocypher, fail-closed, two-directions-tested (`c169cb2`) |
| TorPD: blocked (memory ceiling) | Two-VM boot attempted via `optiplex9020` reference pattern (`9897506`) — **correction, 2026-07-29**: this alone was insufficient. `9897506` introduced a `main.c` patch that itself caused a triple fault (`2d16491`, "WS1 partial" — VMM survives but vm1 still doesn't reach userspace). The actual working fix was `2bc3c7a`, which **reverted** the `main.c` patch entirely (the optiplex9020 reference pattern does not patch main.c) — independently verified: the commit diff contains the raw serial log showing both vm0 and vm1 reaching Buildroot login (`Welcome to Buildroot` / `buildroot login:` each appearing twice, `VM_FATAL=0, triple=0`). |
| GPU-accelerated compositor: not started | Conditional GLES2 path (`c30197a`) + real dual-pass Gaussian blur (`b2d9b08`) |
| Desktop UX (snapping, context menus): not started | Window snapping (`99b69b3`), right-click context menu (`816cb7c`) |
| Apps (editor, task manager, screenshot, browser): not started | Editor/task manager/screenshot landed real (`529a104`, `25014b7`, `cb3daab`). Browser (`5bf6a83`) is **demo-only** — see correction below, its own file's `main()` never fetches anything real. |
| Package repo hosting: none | Real repo, 3 packages + index (`5dd70af`) |

**Lesson applied throughout this doc set:** a PRD/status doc's own "last
verified" date is a floor on its staleness, not a ceiling - always check
`git log` since that date before trusting a "not started" line.

## 2. What's genuinely, independently verified real (source-level, this pass)

- Ed25519 verified boot (fail-closed), `src/boot/wlt-boot/verify.c`
- LUKS dispatch wired to a real `luks.c` (not stub `UNSUPPORTED` returns)
- `wlt-sandbox/launch.c` fail-closed permission-manager init +
  network-only permission gating
- `wlt-ai/tools.c` popen-vs-execv split (`run_command` still shell-based
  and denylist-only; `change_setting` uses real `fork`+`execv`)
- `wlt-init/init-minimal.c` mounts tracefs; `ebpf_engine.c` has real
  `bpf_program__attach()` with a runtime CAP_BPF degraded-mode fallback

See the per-topic docs (`ARCHITECTURE.md`, `SECURITY.md`, `AI_KAIROS.md`,
`BOOT_AND_UPDATES.md`) for the full detail behind each of these.

## 3. What's real per the project's own prior audits (independently spot-checked for reliability, not blindly trusted)

The project has a genuinely good habit of commissioning read-only,
adversarial-style audits of its own claims (orphaned-code audit, A/B-update
audit, disk-encryption audit, Guardian memory-safety/race audits). This
consolidation spot-checked several of their specific claims directly
against source (see §2) and found them accurate every time checked. On
that basis, their other, not-individually-re-checked findings are treated
as reliable secondary sources, distinct from this consolidation's own
primary verification - and are labeled as such throughout the per-topic
docs (e.g. "per the orphaned-code audit" vs. "independently re-read for
this consolidation").

## 4. What's real per git commit messages only (not re-opened/read as source in this pass)

A large volume of app/feature work (see `docs/UI_COMPOSITOR.md` §2,
`docs/PACKAGE_MANAGEMENT.md` §1-2) is recorded here on the strength of
specific, FR-ticket-referenced commit messages plus `git show --stat`
diffs (confirming real files/lines changed, not empty commits) - this is
weaker evidence than a source read but meaningfully stronger than a
self-reported "done" in a doc with no commit backing it.

## 5. Known unresolved gaps (as of 2026-07-29, needing action, not just tracking)

1. **Three unreconciled architecture tracks** (seL4 multi-PD, Track-2
   desktop with no seL4, bare 64-bit seL4) - unifying them is the largest
   open architectural question in the project (ADR-10 Step 3).
2. **Firewall PD does not exist** - Tor-only enforcement today is entirely
   Linux userspace (kill switch), not seL4-level.
3. **`wlt-ai/tools.c run_command` still shells out via `popen`** with only
   an 11-entry denylist - a real, live security gap, not hypothetical.
4. **Only network permission is enforced at launch** - camera/mic/location/
   filesystem/etc. are modeled and persisted by `wlt-permissions` but not
   gated by `wlt-sandbox/launch.c`.
5. **eBPF hook attachment on a real (non-degraded) boot** needs a fresh
   verification pass - the load path and tracefs mount are real, but a
   current serial-log proof of "N hooks attached" was not reproduced here.
6. **TorPD's actual Tor-isolation behavior** (vs. just "the two-VM boot
   works") is unconfirmed in this pass.
7. **`libsodium-dev` availability** for `wlt-update`'s Ed25519 verify at
   build time was an open blocker as of 2026-07-20 and wasn't re-checked.
8. **No completeness re-audit of `wlt-settings`, `wlt-terminal`,
   `wlt-filemanager`, or `wlt-net`** exists at the rigor applied to
   `wlt-update` or the orphaned-code batch - commission one before making
   strong claims about any of the four.
9. **Active RAM/kernel-keyring wipe for amnesia** does not exist - amnesia
   today is passive (tmpfs only).
10. **Tor Browser does not fetch real pages, and its SOCKS5 module is
    orphaned code (found 2026-07-29, source-verified directly)**:
    `wlt-browser.c`'s `main()` never calls any networking function at
    all - it prints a static, hardcoded "architecture demo" string
    (which literally contains the text "SOCKS5 proxy support: tracked
    follow-up") and exercises only its own HTML-stripping/line-format
    functions on that fixed string. Separately, `wlt_socks5.c` (added in
    `e593468`) is a real, correctly-structured RFC 1928 SOCKS5 client -
    but nothing in the codebase calls `wlt_socks5_connect()`, including
    `wlt_http.c`'s own `wlt_http_post()`, which does its own direct
    `gethostbyname()`+`connect()` to the resolved host, bypassing any
    proxy entirely. **No live Tor-bypass risk exists today** (nothing
    fetches anything real yet), but the commit message ("SOCKS5 Tor
    proxy support for wlt_http... real fetch") overclaims - there is no
    real fetch anywhere in this path. Wiring `wlt_http_post` to route
    through an already-established `wlt_socks5_connect()` socket (rather
    than its own direct connect) is the actual remaining work, and it's
    the same bug class (AI egress bypassing Tor, E-C1/C2) this project
    has already found and fixed once elsewhere - check for it explicitly
    when this gets wired, don't assume the earlier fix pattern
    generalizes automatically.

## 6. Phase/workstream framing that's still active

`docs/context/PHASE_SEL4_FINISH_AND_POLISH_ARCHIVED_2026-07-29.md`
(2026-07-28, i.e. written the same day as several of the commits in §1's
table) laid out 4 workstreams - seL4 vm1+Ed25519 finish, desktop UX,
package hosting, and a fourth not enumerated in the doc excerpt reviewed -
in checklist format. Given §1's table shows real completions against at
least 3 of those workstreams within the same day/next few days, this phase
doc's checklist is a reasonable **current** source for "what's being worked
on right now" - re-read it directly (it's archived, not deleted) rather
than relying solely on this summary if planning the next session's work.

## 7. How to keep this document honest going forward

Before adding or trusting any status line in this file:
1. `git log --oneline --since=<this doc's last-updated date>` on the
   relevant path.
2. If a commit claims a fix/feature, `git show --stat <hash>` to confirm
   real files changed (not a docs-only or empty commit).
3. For security-relevant or "is this actually wired in" claims, open the
   actual source file and grep for the specific function/behavior claimed
   - do not trust a commit message alone for those.
4. Date every claim you add.
