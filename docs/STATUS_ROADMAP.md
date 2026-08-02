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
| Verified boot: stub/in-progress | Real Ed25519 crypto exists via monocypher, fail-closed, two-directions-tested in isolation (`c169cb2`) - **correction, 2026-07-29: not actually called during boot**, see gap #11 below. The primitive is real; the boot chain doesn't use it yet, and currently prints a false "Signature verified" regardless. |
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
4. **Only network permission is actually enforced at launch - the new
   filesystem check does not work (found 2026-08-01, source-verified
   directly, see gap #18)** - camera/mic/location/etc. remain entirely
   unmodeled at enforcement time; filesystem was attempted but is a
   no-op due to a real bug.
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
11. **Verified boot is not actually enforced at boot time, and the
    bootloader prints a false success message (found 2026-07-29,
    source-verified directly - this is more serious than "unwired")**:
    `src/boot/wlt-boot/boot.c`'s `efi_main()` has, inside
    `if (config.verify_signature)`: a commented-out call to
    `wlt_verify_kernel` and its error check, followed by an
    UNCONDITIONAL, NOT-commented-out `wlt_console_info("Signature
    verified")`. This means the boot console prints "Signature
    verified" regardless of whether any verification occurred - which
    today is always, since nothing runs. Neither `wlt_verify_kernel`
    (SHA-256) nor the new, real `wlt_verify_ed25519` (monocypher,
    confirmed correct in isolation - see the Boot Verifier PD and
    verified-boot entries above) is called anywhere in `boot.c`. The
    kernel-jump step immediately after is also a bare TODO stub -
    `efi_main` does not yet transfer control to a kernel at all, so this
    is an early-stage bootloader skeleton, not a functioning boot chain.
    **Do not describe "verified boot" as done or enforced until this is
    fixed** - the crypto primitive being correct does not mean the
    boot process uses it. Fix: replace the commented-out block with a
    real call to `wlt_verify_ed25519` (the fail-closed, real-crypto
    path), gate the success message on its actual return value, and
    do not print "Signature verified" on any path that didn't call it.
12. **Package downloads over Tor are real for wlt-pkg specifically
    (2026-07-30, source-verified)**: `download.c`'s `wlt_pkg_download()`
    correctly tunnels - raw connect to 127.0.0.1:9050, a real SOCKS5
    handshake (domain-name ATYP, no local DNS resolution) to the target,
    then the HTTP GET over that same already-tunneled socket. This is a
    separate, self-contained implementation from the orphaned
    `wlt_socks5.c`/browser gap in item #10 above - it fixes package
    downloads specifically, it does NOT fix the browser/AI path, which
    remains orphaned as described in #10.
13. **Tor-in-vm1 claim needs clarification, not yet fully confirmed**:
    a report states "[tor-pd] Starting Tor daemon..." appeared in "BOTH
    VMs" during a boot test. If literal (Tor running in vm0 as well as
    vm1), that would defeat the isolation point of TorPD entirely -
    more likely this just means each VM independently printed its own
    boot-log line, but this hasn't been confirmed either way against a
    raw log. Get a direct answer on this before treating Tor-in-vm1 as
    settled.
14. **FR-GUI-11 (context menu) is not actually a context menu (found
    2026-07-30, source-verified directly)**: `816cb7c`'s own commit
    message is honest about the mechanism ("Plain right-click cycles:
    maximize -> restore -> minimize") - the code genuinely does that,
    no fabrication. But this doesn't satisfy the requirement as asked:
    there is no menu surface rendered at all (no visible list of
    options, no click-to-select) - it's a stateless right-click cycling
    shortcut mislabeled with the FR-GUI-11 tag. Additionally, `ctx_state`
    is a single global `static int`, not per-window - right-clicking
    window B continues wherever window A's clicks left the counter,
    rather than each window cycling independently, which would feel
    broken to a real user (a freshly-clicked window doesn't reliably
    start at "maximize"). FR-GUI-11 still needs a real menu surface
    built - this entry should not be read as "done."
15. **RESOLVED 2026-07-31 (`9a2200f`), independently re-verified**: the
    backslash-path bug in `boot.c`'s pubkey load is fixed - confirmed by
    direct read, line 87 now reads
    `wlt_loader_load("/boot/keys/boot-pubkey.bin", &pubkey_img)`,
    matching the surrounding comment/error-message convention.
16. **RESOLVED 2026-07-31 (`9a2200f`), independently re-verified**:
    `wlt-browser.c` now genuinely calls `wlt_http_get_tor()` (confirmed
    by direct read, line 86) - the hardcoded demo string is gone, the
    browser fetches real pages through Tor SOCKS5.
17. **RESOLVED 2026-07-31 (`9a2200f`), independently re-verified**:
    `file_dnd.c` is genuinely in `wlt-filemanager/Makefile`'s `SRCS`
    (confirmed by direct read), and `filemanager.c`'s keyboard handler
    genuinely calls `file_dnd_mark()`/`file_dnd_paste()` on real
    keypresses (case 50 / case 33, confirmed by direct read) - no
    longer an orphaned module.
18. **`launch.c`'s filesystem permission enforcement (Section C,
    `6837096`) is a real bug, not real enforcement - it modifies a
    struct that's discarded before the sandboxed process is ever set up
    (found 2026-08-01, source-verified directly by reading the full
    function)**: the DENY-override logic operates on a *local* variable
    - `wlt_SandboxManifest mfst;` loaded once, at the top of
    `wlt_sandbox_launch()`, purely for the permission check. On DENY it
    sets `mfst.filesystem = WLT_FS_NONE;` - but `mfst` goes out of scope
    at the end of that block and is never referenced again. The actual
    sandboxed child is set up later via `child_setup(app, ...)` ->
    `wlt_sandbox_setup_filesystem(&app->manifest, ...)`, where
    `app->manifest` is a **separate struct**, loaded independently
    inside `prepare_app_slot()` (`wlt_manifest_load(manifest_path,
    &app->manifest)` - same path string, different memory, called after
    the permission-check block already finished). The override on
    `mfst` has no path to `app->manifest` at all. Net effect: the DENY
    decision is correctly detected, correctly logged
    ("PERMISSION DENIED: ... filesystem ... DENY"), and correctly
    audit-logged - and then does nothing. An app with a DENY filesystem
    grant launches with its full originally-manifested filesystem access
    regardless. Contrast with the network check three lines above it in
    the same function, which is real: on DENY it `return`s and blocks
    the launch outright, rather than trying to mutate a field that gets
    thrown away. **Do not read filesystem permission enforcement as done
    until this is fixed.** Fix: either move this check to after
    `prepare_app_slot()` runs (so `app->manifest` exists) and mutate
    `app->manifest.filesystem` directly, or carry the DENY decision
    forward as an explicit flag and apply it to `app->manifest` before
    `child_setup()` is called - either way, the override has to land on
    the struct that `wlt_sandbox_setup_filesystem()` actually reads.
19. **`wlt-update/rollback.c`'s `wlt_rollback_load()` reads an
    uninitialized buffer with `strlen()` on a truncated counter file
    (found 2026-08-01, source-verified directly)**: `hmac_line` is
    declared `char hmac_line[HMAC_HEX_LEN + 2];` with no initializer.
    If the counter file has only one line (e.g. corruption, or a
    crash mid-write predating the atomic-write fix), the second
    `fgets(hmac_line, ...)` returns `NULL` and the buffer is never
    touched - but the code unconditionally proceeds to
    `strlen(hmac_line)` regardless of whether `fgets` succeeded. This
    is genuine undefined behavior (reading indeterminate stack memory
    as a C string). Lower severity than gaps #11/#18 - the likely
    practical outcomes are a spurious fail-closed rejection (garbage
    almost certainly won't match a real 64-hex-char HMAC) or a crash,
    not a silent bypass, since the surrounding logic already fails
    closed on any HMAC mismatch. Still a real bug worth a one-line
    fix: check `fgets`'s return value directly (e.g.
    `if (fgets(...) == NULL) { treat as missing HMAC }`) rather than
    inferring "line present" via `strlen()` on a possibly-untouched
    buffer. The rest of this file - the atomic temp+fsync+rename+
    dir-fsync write in `wlt_rollback_store()`, the monotonic-only
    `wlt_rollback_check()`/`wlt_rollback_bump()` comparison logic, and
    the fail-closed HMAC verification - is correct on direct read. The
    hardcoded build-time HMAC key is a separately, honestly
    self-documented limitation in the file's own comments (marked as
    needing KeyVaultPD delivery) - not a new finding.
20. **`wlt-update/verify.c`'s `wlt_update_compute_sha256()` does not
    compute SHA-256 - it computes BLAKE2b via libsodium's
    `crypto_generichash` (found 2026-08-01, source-verified directly,
    likely low current impact but a real landmine)**: the function name,
    doc comment ("Compute the SHA-256 of a file"), and its output field
    (`wlt_UpdateManifest.sha256_hex`) all say SHA-256; the actual call is
    `crypto_generichash_init/update/final`, which is BLAKE2b by default
    in libsodium, not SHA-256. Not a weaker hash (BLAKE2b is a legitimate,
    secure choice) - a naming/labeling mismatch. Checked for live impact:
    the only caller found is `update.c:655`, and no dedicated external
    manifest-signing script for `wlt-update` (as opposed to `wlt-pkg`,
    which is a separate system) was found in `scripts/` - so this is
    likely self-consistent today (nothing yet compares it against a
    real, independently-computed SHA-256). The real risk is future: if a
    release-signing pipeline gets built expecting the field name
    `sha256_hex` to mean what it says, it will naturally compute real
    SHA-256 and every legitimately-signed package will fail this check.
    Fix: either rename the field/function to reflect BLAKE2b honestly, or
    switch the implementation to genuine SHA-256 if external
    interoperability with standard tooling (`sha256sum`) is ever needed.
21. **Guardian's irreversible-action confirmation token can never be set
    - the variable holding it has function-local scope (found 2026-08-01,
    source-verified directly)**: `action_engine.c`'s `g_action_apply()`
    gates irreversible actions (kill, secure delete, kill switch) behind
    `strcmp(getenv("GUARDIAN_CONFIRM_TOKEN"), g_confirm_token)`, where
    `g_confirm_token` is declared `static char g_confirm_token[64] = {0};`
    **inside the function body itself** - a function-local static. In
    standard C this has function-local visibility even among static
    functions in the same file; no other function, anywhere, in any file,
    can ever write to it. Confirmed via `grep -r g_confirm_token` across
    all of `wlt-defender/src/`: exactly 2 occurrences, both on adjacent
    lines inside this one function - no setter exists. The comment
    directly above it describes an architecture that isn't implemented by
    this code: "The token is set via the API socket by the user (kairos
    confirm <token>) and stored in a static variable" - that setter was
    never written. Net effect: the variable is permanently zero-initialized
    empty. `strcmp(confirm, "")` only succeeds if `GUARDIAN_CONFIRM_TOKEN`
    is set to an empty string (`export GUARDIAN_CONFIRM_TOKEN=` with
    nothing after `=`) - a low-probability but real accidental-bypass
    edge case for a gate protecting SIGKILL/secure-delete/kill-switch
    actions. In practice this means the intended replacement for the old,
    already-flagged-insecure `GUARDIAN_AUTOCONFIRM` env var (Bug 25's own
    fix comment references this) doesn't actually work yet - the only
    currently-functional path to confirm an irreversible action is the
    explicitly-labeled-as-legacy `MCP_AUTOCONFIRM` fallback. Fix: give
    `g_confirm_token` file or global scope, and actually implement the
    "kairos confirm <token>" API-socket setter the comment describes -
    until then this gate provides materially less protection than its own
    documentation claims.
22. **`ransomware_engine.c`'s per-pid tracking table fills up permanently
    and silently stops detecting new processes (found 2026-08-01,
    source-verified directly)**: `find_slot()` maintains a fixed 64-entry
    table (`re->slots[64]`) of PIDs it's tracking deltas for, growing via
    `re->n++` on every previously-unseen pid - but nothing anywhere in this
    file ever removes an entry, including for processes that have long
    since exited. Once 64 *distinct* PIDs have been observed since the
    daemon started (cumulative over its entire uptime, not concurrent),
    `find_slot()` returns `-1` for every subsequent new pid, and the only
    caller (`g_ransomware_check`'s scan loop) does `if (si < 0) continue;`
    - no log line, no alert, no metric of any kind marking that tracking
    capacity has been exhausted. On a real desktop OS where the shell
    spawns a new short-lived process per command, this cap is reachable
    within hours of uptime, after which bulk-delete/rename-storm/high-
    write-rate ransomware detection silently stops functioning for every
    process spawned from that point forward, with nothing anywhere
    indicating detection coverage has degraded. The delta-based rate
    calculation itself (this file's own documented "Bug 4 fix," replacing
    a cumulative-total false-positive problem) is sound and correct on
    direct read - this is a capacity/eviction bug layered on top of an
    otherwise-correct detection algorithm. Fix: evict stale slots (check
    `/proc/<pid>` still exists, or track last-seen time and evict oldest
    on table-full) rather than growing unbounded-but-capped with no
    reclamation, and log/alert when the table is full rather than silently
    dropping coverage.
23. **`net_engine.c`'s `/proc/net/{tcp,tcp6,udp,udp6}` parser appears to
    silently fail on every line, meaning the Tor-leak-detection engine
    may have never actually detected anything (found 2026-08-01,
    source-verified directly - this is the most severe finding of this
    review pass, verify the sscanf analysis independently before
    treating it as certain, but the reasoning below is precise, not
    a guess)**: the Linux kernel formats each `/proc/net/tcp`-family line
    with `tx_queue` and `rx_queue` joined by a colon into ONE token
    (`%08X:%08X`), and likewise `tr`/`tm->when` (`%02X:%08lX`) - not as
    independent space-separated numbers. `check_proto_uid()`'s parser -
    `sscanf(line, "%*d: %63s %63s %15s %lu %lu %lu %lu %u %u %llu", ...)`
    - treats these as 4 separate `%lu` fields. Tracing the match: the
    first `%lu` reads up to the internal colon in `tx_queue:rx_queue` and
    stops (colon isn't numeric); the next `%lu` must then consume the
    leftover `:00000000..`, but `%lu` only skips leading *whitespace*, not
    a colon, so it fails to match immediately. `sscanf` returns 4
    (`local`, `remote`, `state`, plus the truncated `tx_q`) - under the
    `if (n < 8) continue;` threshold - so every line in every one of the
    four proc files is silently skipped, every call, unconditionally.
    `g_net_check_leak()` therefore always returns "no leak" /
    `G_VERDICT_CLEAN` regardless of actual traffic. This is specifically
    the engine meant to catch the exact bug class (non-Tor egress /
    DNS-style leaks) this project has already found real instances of
    twice elsewhere (AI egress, orphaned browser SOCKS5) - if this
    analysis is correct, the detection layer meant to catch a THIRD
    instance of that bug class has itself been silently non-functional.
    Fix: parse `local`/`remote`/`state` as strings (already correct),
    then parse the two colon-joined fields as strings too (or with
    `%*x:%*x` to discard them) before reading `uid` - do not assume
    space-separated where the kernel emits colon-separated.
24. **Separately in the same file: loaded Tor consensus guard-node IPs
    are never consulted (found 2026-08-01, source-verified directly)**:
    `load_tor_consensus()` genuinely parses `/var/lib/tor/cached-
    consensus` and populates `ne->guard_ips[]`/`ne->n_guard_ips` - real
    file I/O, real parsing. But neither `check_proto_uid()` nor
    `g_net_check_leak()` ever reads `guard_ips`/`n_guard_ips` - the
    "flag Tor connections to non-guard IPs" feature described in the
    struct's own comment ("Phase 4.4... flagged SUSPICIOUS_TOR_
    DESTINATION") is loaded into memory and then never used. Same
    build-but-not-wired pattern as gaps #10, #16, #17 earlier in this
    document.
25. **`92fe3cc`'s UEFI handoff (bootloader Section C) has two real issues,
    one high-confidence, one flagged with appropriate hedging (found
    2026-08-02, source-verified directly)**: `boot.c`'s new handoff code
    calls `ExitBootServices` via a retry loop, then unconditionally jumps
    to `kernel.entry_point` right after. (1) **High confidence**: there is
    no check of the loop's final status before jumping - the loop does
    `if(!s) break;` on success, but falls through after 8 failed attempts
    with no subsequent check of `s`. If `ExitBootServices` never succeeds
    (a real UEFI-spec behavior when the memory-map key is stale, which is
    the likely outcome of issue below), boot services remain active when
    the kernel starts - undefined/catastrophic for any real OS kernel
    that assumes firmware has handed off control. Compounding this: the
    memory-map buffer is fixed at 16KB with no growth path - if
    `GetMemoryMap` returns "buffer too small" (realistic on systems with
    many memory regions), the loop retries the same undersized buffer up
    to 8 times rather than allocating the size the firmware actually
    reported needing, then proceeds regardless. (2) **Flagged, not
    asserted with full confidence**: `BootServices`/`ExitBootServices`/
    `GetMemoryMap` are accessed via hardcoded raw byte offsets
    (`+96`/`+208`/`+32`) into the system/boot-services tables instead of
    real typed struct definitions (e.g. from `gnu-efi`'s `<efi.h>`).
    Offset 96 for `BootServices` within `EFI_SYSTEM_TABLE` checks out
    against the standard UEFI x86_64 layout on manual recount. The other
    two offsets are less certain from memory alone and may not match the
    real `EFI_BOOT_SERVICES` layout - this needs verification against the
    actual UEFI spec or `gnu-efi` headers, not taken on faith either way.
    Independent of whether these specific numbers are right, hardcoding
    raw offsets into a firmware ABI struct instead of using real struct
    definitions is inherently fragile and effectively unverifiable by
    inspection - recommend switching to typed definitions regardless of
    how the offset check-out resolves. Note: build succeeding (`wlt-boot.
    efi`, 66864 bytes) only confirms the code compiles and links - it has
    not been boot-tested against real or emulated UEFI firmware, so
    neither of these issues would have been caught by the build step
    alone.

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
