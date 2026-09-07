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
    thrown away. ~~Do not read filesystem permission enforcement as done
    until this is fixed.~~ **FIXED 2026-08-24**, independently
    re-derived by tracing the live code before finding this entry (good
    convergent confirmation the original analysis was right): added an
    `fs_deny_override` flag in `wlt_sandbox_launch()`'s outer scope, set
    it instead of the discarded `mfst.filesystem` write, and applied it
    to `app->manifest.filesystem` immediately after `prepare_app_slot()`
    returns - the first point in the function where that struct exists.
    `child_setup()` -> `wlt_sandbox_setup_filesystem()` now reads the
    actually-overridden value. This had been sitting unfixed since the
    original 2026-08-01 finding - worth noting as a case where a
    correctly-documented bug still needs someone to circle back and
    apply the fix, documentation alone doesn't close the gap.
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
    needing KeyVaultPD delivery) - not a new finding. **FIXED**, per
    #29 above (`10aa19b`, 2026-08-04) - `hmac_line` is zero-initialized
    before the `fgets` call now. Cross-referencing here so this entry
    doesn't read as still-open in isolation.
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
    documentation claims. **FIXED**, per #44 above (2026-08-16) -
    `g_confirm_token` now rejects an empty stored value regardless of
    the env var, closing the accidental-bypass edge case. The deeper
    issue this entry raises - no setter for `g_confirm_token` exists
    anywhere, so the "kairos confirm <token>" flow this comment
    describes was never actually implemented - is still true and still
    open; #44's fix makes the gate fail closed in its current
    permanently-empty state, it doesn't implement the missing setter.
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
    dropping coverage. **FIXED**, per #49 above (2026-08-16) - added
    dead-PID slot reclamation via `kill(pid,0)`/`ESRCH` when the table is
    full, matching exactly the first fix option this entry suggested.
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
    space-separated where the kernel emits colon-separated. **FIXED**,
    per #29 above (`8f2fc25`, 2026-08-04) - confirmed via `git show`
    against the actual diff, not just the commit message. Also #47
    (2026-08-16) separately hardened this same file's `find_tor()`
    retry behavior and raised an open architectural question about
    whether this engine's UID-matching approach is even reachable given
    Tor's confirmed separate-VM placement (#53) - worth reading
    together with this entry, not just this one in isolation.
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
    document. **FIXED 2026-08-24**: added a guard-IP check inside the
    existing `if (uid == ne->tor_uid)` branch in `check_proto_uid()` -
    only tightens when `n_guard_ips > 0` (falls back to the original
    trust-tor-uid behavior if consensus hasn't loaded, to avoid false
    positives with no reference data), flags `SUSPICIOUS_TOR_DESTINATION`
    for a non-guard remote IP. Verified the byte-order comparison is
    valid before writing it: `raddr_hex` (parsed via `%X` from
    `/proc/net/tcp`) and `guard_ips[]` (populated from `inet_aton`'s
    `s_addr`) are both raw network-byte-order bits with the same
    integer representation on this target - confirmed against this
    file's own worked example in a comment above ("0100007F =
    127.0.0.1"), not assumed.
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
    alone. **FIXED (compile-verified), NOT boot-verified** - per `05a52ec`
    (2026-08-02, confirmed via `git show` at the very start of this
    session): vendored real `efi.h` from gnu-efi (closing issue 2
    entirely - real typed definitions now, no more raw offsets), and
    added the missing `ExitBootServices` return-value check plus dynamic
    `AllocatePool` sizing (closing issue 1's two components). What's
    still genuinely open, flagged at the very start of this session and
    still true now: the commit's own message only confirms
    `wlt-boot.efi` compiles and links - an actual OVMF/real-hardware boot
    test confirming this handoff works at runtime has not been done.
    Queued as Section D of the PHASE MUSL-TOR handoff earlier this
    session for exactly this reason.
26. **Firewall PD's real source files (`d7f68e7`) exist and are honestly
    scoped, but aren't in the tracked repo - reproducibility gap, not a
    content problem (found 2026-08-02, verified by reading the actual
    file on disk)**: `firewall_pd.c` genuinely exists at `sel4-build/
    projects/vm/components/FirewallPD/src/` and matches its description
    - real ping/pong IPC, real kill-switch state, packet filtering
    honestly scoped as prefix-matching for the IPC-proof stage rather
    than overclaiming real IP/TCP/UDP parsing. But the commit itself only
    contains two shell scripts (`create_firewall_pd.sh`/`sync_fw.sh`) -
    the actual `.camkes`/`.c`/`CMakeLists.txt` files live only in the
    gitignored `sel4-build/` tree. Same pattern as TorPD's very first
    scaffold attempt early in this project. Not a security issue, but a
    real reproducibility one: delete `sel4-build/` and this doesn't come
    back without re-running the generator script by hand. **FIXED**, per
    #30 above (`96a5c13`, 2026-08-02) - both Firewall PD's and TorPD's
    camkes files committed under `src/sel4-partitions/`, same day this
    was found.
27. **ADR-09 (VM-to-VM IPC across the seL4 boundary) is confirmed working,
    not just attempted (`6157070`, 2026-08-05)** - the single most-chased
    proof in this project. Real ping/pong succeeds in both directions via
    shared memory at the crossvm BAR1 addresses; serial log shows RESULT
    SUCCESS on both vm0 and vm1. (found 2026-08-16, verified via `git show
    6157070`)
28. **musl-compiled Tor runs in vm1 without segfaulting (`eac1c52`,
    2026-08-08), but end-to-end SOCKS5-through-bridge is not yet closed
    out**: the glibc build hit an unresolvable seL4-EPT/AVX2 dead end
    (documented and reverted in `c76e84e`, same day); switching to a
    musl/Alpine build fixed the segfault entirely, and the bridge
    ping/pong from #27 still works with Tor running on top of it.
    Follow-on commits through `d734818` (2026-08-09) show real forward
    progress - SOCKS5 port reachable, a pcnet NIC/DSDT fix landed, "C.1
    SOCKS5 PASS" - but the most recent TorPD-specific commit is
    diagnostic-only: it adds `printf` tracing to the PCI config-space
    (0xCF8/0xCFC) read/write path for the pcnet device and states the
    root cause is "identified," but contains no behavioral fix and no
    boot-test log confirming one. Needs an actual boot test to read the
    new trace output before this can be called done. (found 2026-08-16,
    verified via `git show d734818` - diagnostic-only, no fix applied)
29. **Two gaps this document already listed (#19, #23) were fixed and
    never checked off (`10aa19b`, `8f2fc25`, both 2026-08-04)**:
    `rollback.c`'s zero-init UB and `net_engine.c`'s `/proc/net/tcp`
    parser silently failing on every line (the "most severe finding"
    framing in #23) are both resolved per the actual diffs, not just the
    commit messages. (found 2026-08-16, verified via `git show` on both)
30. **Gap #26 (Firewall PD reproducibility) was fixed the same day it was
    logged (`96a5c13`, 2026-08-02)**: TorPD's and Firewall PD's camkes
    files, previously gitignored-only, are now committed under
    `src/sel4-partitions/`. (found 2026-08-16, verified via `git show
    96a5c13`)
31. **Three complete, spec-compliant libraries exist and compile, but
    nothing in the tree links or calls any of them (verified via
    repo-wide grep across every Makefile/CMakeLists.txt/shell script,
    2026-08-16)**: `libwlt-settings.a` (4 real config panels -
    network/display/security/privacy - behind a typed, validated,
    listener-based engine in `settings.c`) and `libwlt-shell.a`
    (`taskbar.c`/`launcher.c`/`notifications.c`/`tray.c`) are each
    referenced in exactly one place in the whole repo: their own
    Makefile's build target. The binaries that actually ship -
    `wlt-settings-app.c` and `wlt-statusbar.c` - are separate, simpler
    reimplementations that never `#include` the library headers.
    `wlt-settings-app.c` is worse than "just unwired": its glyph table
    (`font5x7[96][7]`) is declared but literally all-zero and never used
    for rendering, so the window shows colored blocks, not text, and it
    binds no `wl_seat`, so nothing in it is interactive - a read-only,
    illegible settings app. `wlt-statusbar.c` is the more honest case: a
    real, populated bitmap font, and its own comment explains the
    simplification ("the compositor doesn't expose its view list via
    Wayland protocol yet"). Separately, `wlt-term-wayland.c` includes
    system `<pty.h>` (glibc's `forkpty()`) rather than the local `pty.h`
    declaring the project's own richer `wlt_pty_*` API in `pty.c` - same
    disconnection pattern, third occurrence. `wlt-terminal`'s Makefile
    had an independent bug on top of this: its archive recipe referenced
    an undefined `$(OBJS)` instead of `$(LIB_OBJS)`, so `libwlt-
    terminal.a` had never successfully built at all (fixed directly,
    2026-08-16 - pure variable-name correction, safe without a
    compiler). Also worth noting: `wlt-settings/Makefile`'s own comment
    claims "the compositor... pull[s] in" the settings library - that's
    actively false per the grep, not merely silent about it, unlike
    `wlt-terminal`'s Makefile, which is honest that its library is "the
    spec'd implementation" separate from "the actual running binary."
    Not yet re-audited: whether this pattern holds for `wlt-net` or
    `wlt-filemanager` (gap #8's original ask) - `wlt-net` at least ships
    a single real `wltnet` binary built directly from all its `.c`
    files, no parallel "-app" reimplementation, so it does not appear to
    share this specific pattern.
32. **`kill_switch.c` full read: architecture is genuinely sound
    (fail-closed everywhere, explicit ordering rationale in comments,
    Tor-only-install failure falls back to block-all rather than failing
    open); one real hardening gap found and fixed (2026-08-16)**: both
    backends' "allow replies from Tor services" INPUT rule matched on
    `saddr 127.0.0.1` alone, with no interface restriction, unlike every
    other rule in the file (all of which use `iifname "lo"` / `-i lo`).
    Standard Linux kernel behavior already drops martian 127.0.0.0/8-
    sourced packets arriving on non-loopback interfaces before iptables/
    nftables ever sees them, so this was very unlikely independently
    exploitable - but the file's stated purpose is defense-in-depth
    against exactly this class of assumption, so it shouldn't have
    silently depended on one anyway. Added `iifname "lo"` / `-i lo` to
    both rules - strictly tightening, cannot reject traffic that
    currently works. Remaining unaudited in `wlt-net`: `tor_proxy.c`,
    `net.c`, `wifi.c`, `ethernet.c`, `mac.c`. `wlt-filemanager` not
    started.
33. **`wlt_net_init()` installed block-all on every CLI invocation,
    including read-only ones like `status` - found + fixed 2026-08-16**:
    `wlt_kill_switch_install()` ran before `wlt_tor_proxy_init()`, so it
    always branched on the un-probed default `tor_state` rather than
    Tor's real status, and per its own logic that meant apply_block()
    every time regardless of whether Tor was actually up. No non-`daemon`
    subcommand ever called kill_switch_release(), and `wlt_net_shutdown()`
    deliberately re-applies block on exit (intentional, from an earlier
    fix) - so running e.g. `wltnet status` while the daemon is up would
    silently strand the whole system in block-all: the daemon's own
    in-memory `kill_switch_active` never gets told an external process
    touched the shared OS firewall state, so it has no way to detect or
    self-correct. Fixed the ordering - `wlt_tor_proxy_init()` now runs
    before the install decision - a pure move, safe without a compiler,
    restoring what `wlt_kill_switch_install()`'s own logic already
    intended. NOT fully closed: `wlt_net_shutdown()` still unconditionally
    re-blocks on exit for every subcommand, including ones that never
    should have touched firewall state at all (a `status` query shouldn't
    leave the box more locked down than it found it). Real fix needs
    `main()`'s dispatch restructured so read-only subcommands skip the
    full install/shutdown firewall lifecycle - build-verification-
    required, left for GLM rather than attempted blind.
34. **`wlt_wifi_connect`'s quote-rejection fix (`c09979a`) didn't cover
    backslash - found + fixed 2026-08-16**: same unescaped-embedding risk
    the quote check already exists to close; wpa_supplicant's
    config-value parser treats `\` as an escape introducer, so an
    SSID/PSK containing one could alter what closing-quote position - or
    what value - actually gets stored. Extended the same fail-closed
    rejection to `\`, matching the existing fix's own logic. Also
    checked and ruled out: `wlt_wifi_forget`'s
    `sscanf(..., "%63[^\t]", ...)` into a `WLT_NET_MAX_SSID`-sized field
    looked like a hardcoded-width mismatch at a glance - it isn't;
    `WLT_NET_MAX_SSID` is 64, so 63 chars + null fits exactly. Remaining
    unaudited in `wlt-net`: `ethernet.c`, `mac.c`. `wlt-filemanager` not
    started.
35. **`wlt_ethernet_configure_static`'s netmask-to-prefix conversion
    accepted non-contiguous netmasks silently - found + fixed
    2026-08-16**: the leading-ones-counting loop stops at the first
    0-bit and discards anything after, so a malformed netmask like
    `255.0.255.0` would silently compute a truncated, wrong prefix
    rather than erroring. Added a reconstruct-and-compare check that
    rejects anything that isn't a proper contiguous mask. `mac.c`
    reviewed in full - no findings, genuinely clean. **`wlt-net` is now
    fully audited (all 6 files: kill_switch.c, tor_proxy.c, net.c,
    wifi.c, ethernet.c, mac.c) at the rigor gap #8 asked for.** Only
    `wlt-filemanager` remains open from that gap.
36. **`wlt-filemanager` was never linked into a runnable binary - found +
    fixed 2026-08-16**: the Makefile only ever archived
    filemanager.c/operations.c/config.c/file_dnd.c into
    libwlt-filemanager.a; no target ever produced an executable, despite
    xdg-shell-client-protocol.c sitting right there for exactly that
    purpose. Unlike wlt-settings/wlt-shell/wlt-terminal, there's no
    competing simpler reimplementation shipping in its place -
    filemanager.c is genuinely complete (real main(), real event loop,
    real wl_seat/keyboard wiring, a real populated font, working
    directory navigation) - nothing was shipping at all. Added the
    missing binary target using the same pkg-config pattern already
    proven in wlt-terminal's Makefile. Not build-verified by Claude
    directly (no compiler access on this side), but `git status` now
    shows a `wlt-filemanager` binary present on disk where none existed
    before - real evidence a build was run against it, even though
    "zero warnings" itself isn't independently checkable without running
    the build directly.
37. **`wlt_fm_copy_directory`/`wlt_fm_delete_directory` followed symlinks
    during recursion - found + fixed 2026-08-16, most severe finding this
    session**: both used `wlt_fm_is_directory()` (stat-based, follows
    symlinks) to decide whether to recurse into an entry. For delete,
    this meant a folder containing a symlink pointing anywhere else on
    the filesystem - a parent, /, /home, anywhere - would have the
    *target's* contents recursively destroyed by an ordinary "delete this
    folder" action, unbounded by what the user actually selected; also
    reachable via `wlt_fm_move()`'s cross-filesystem (EXDEV) fallback,
    and via a top-level symlink path directly (`opendir`/`rmdir`
    semantics meant the target's contents got wiped even though the
    final `rmdir` on the symlink path itself would fail). For copy:
    unbounded subtree duplication, or an infinite loop on a cycle. Fixed
    with `lstat`-based symlink detection at both the recursion
    entry-point and the per-entry level - copy recreates the symlink
    itself (matches `cp -r`), delete unlinks it directly (matches
    `rm -rf`), neither dereferences. Gap #33 (`wlt-net`'s kill-switch
    bug) was about accidentally blocking your own traffic; this one could
    destroy data outside the folder a user selected, unprompted - worse.
38. **`file_dnd.c` duplicates a worse version of `wlt_fm_move()` instead
    of calling it - found, not fixed, 2026-08-16**: its own
    rename-then-copy-fallback doesn't preserve source permissions
    (`fopen(dst,"wb")` vs `wlt_fm_copy_file`'s `st.st_mode`-aware
    `open()`), and doesn't route through the now-hardened directory-aware
    logic at all - nothing in `kb_key` stops a directory from being
    marked, and pasting one relies on `rename()` succeeding; the EXDEV
    fallback only handles single files via `fopen`, which fails safely
    (not silently) on a directory rather than corrupting anything, but
    the whole function is redundant with, and worse than, `wlt_fm_move()`.
    Recommend `file_dnd_paste` just call `wlt_fm_move()` - didn't do this
    refactor now since it's a maintainability improvement, not an
    active-danger fix like #37, and it touches call signatures across two
    files. `config.c` reviewed in full - clean; `shred_passes` isn't
    validated at parse time but `wlt_fm_secure_delete`'s own `passes < 1`
    guard already defends against it, which is adequate. **`wlt-filemanager`
    is now fully audited - gap #8 is completely closed (wlt-net +
    wlt-filemanager, every file).**
39. **`tor_proxy.c` METHOD-byte validation + control-port poll() timeouts
    - found + fixed by GLM 2026-08-16 (`433d444`), independently verified
    here via `git show`**: `tor_socks_greeting()` previously only checked
    `resp[0]==0x05` (VER), not `resp[1]` (METHOD) - a Tor SOCKS port that
    rejected the greeting (METHOD=0xFF) or required auth (METHOD=0x02)
    would be wrongly declared READY, a false positive that could release
    the kill switch while Tor can't actually proxy. This is a genuine
    catch Claude's own tor_proxy.c pass missed - judged the VER-only
    check "correct for a minimal health-check" at the time; it wasn't.
    Also added: `poll()` with a 2s timeout before both control-port reads
    in `wlt_tor_proxy_new_identity()` (previously blocking, could hang
    the caller indefinitely on a stalled control port), and
    `strncmp(buf,"250",3)` replacing the looser `strstr` match, on both
    control-port checks. Diff matches the commit message exactly -
    verified line by line, not just the message. Separately: `git status`
    confirms gaps #33-38 are all intact on disk as uncommitted changes,
    none clobbered by this or any other concurrent work.
40. **`wlt-defender` (Guardian) audit started - `guardian.c` (1619 lines)
    read in full, 2026-08-16**: dramatically more mature than anything
    else audited this session. Extensive prior hardening already in
    place, evidenced by ~20+ numbered "Bug N fix" comments (dedup/
    rate-bucket eviction that used a frozen counter as its modulo base,
    now a separate monotonic write index; a subscriber-list deadlock
    avoided by copying the list under lock then releasing before
    calling; SO_PEERCRED-based API socket auth that fails closed if
    credentials can't be read; an accept-then-read race fixed with a
    bounded retry loop), plus 14 self-tests covering dedup,
    rate-limiting, ML model hash rejection, log quota overflow, FIM
    encryption+tamper detection, API auth, memory scanning, canary
    restart false-positives, isolation-forest inference correctness, and
    ransomware rate-vs-cumulative detection. Found 2 real issues despite
    this: (1) `g_get_log_key()`'s `/dev/urandom` failure path silently
    used a hardcoded, universally-known key (0x55 repeated) with no
    warning - inconsistent with this same file's FIM key generation a
    few hundred lines later, which correctly refuses a deterministic key
    and warns instead. Fixed to warn loudly, matching the FIM pattern
    (didn't restructure the call chain to skip log encryption entirely -
    would need a signature change touching every call site). Low
    real-world likelihood (`/dev/urandom` essentially never fails on
    Linux) but free to fix. (2) The self-test's isolation-forest
    fallback path has a hardcoded developer-machine absolute path -
    harmless debug leftover, self-test-only, not fixed (cosmetic).
41. **eBPF/userspace FNV-1a hash mismatch for high-bit bytes - found +
    fixed 2026-08-16**: `bpf/guardian.bpf.c`'s `hash_str()` used
    `char c` (signed on this platform) in `h ^= c`, sign-extending any
    byte >= 0x80 to `0xFFFFFFFFFFFFFFFF` before the XOR instead of the
    intended 0-255 value - genuinely diverges from `guardian.c`'s
    userspace hash (which explicitly casts to `unsigned char`), despite
    a comment on `hash_str()` requiring the two to match exactly.
    Verified by reading both implementations side by side, not just
    trusting the comment. No current whitelist entry (passwd/login/
    sshd/pam_unix/chpasswd/gpasswd/su/sudo) contains a high-bit byte, so
    this causes no failure today - calibrating accordingly, not
    overclaiming a live exploit. Fixed with a cast, matching the pattern
    already correctly used in the sibling `hash_comm()` function a few
    lines below (the variant actually used for the real-time comm-name
    lookup against the whitelist, per its own "BUG 1 fix" comment - so
    the higher-stakes path was already correct; `hash_str()` had
    drifted).
42. **`wlt-defender` is a much larger module than anything else audited
    this session** - 16 source files (`action_engine.c`,
    `anomaly_engine.c`, `canary_engine.c`, `ebpf_engine.c`,
    `fim_engine.c`, `forensic_log.c`, `guardian.c`, `iforest.c`,
    `kairos_cli.c`, `mcp_server.c`, `net_engine.c`, `pmu_engine.c`,
    `policy_engine.c`, `ransomware_engine.c`, `sig_engine.c`,
    `watchdog.c`) plus an eBPF program, a Python MCP server, and an ML
    pipeline. Only `guardian.c` and one targeted function pair in
    `bpf/guardian.bpf.c` are covered so far - this is a start, not a
    completed audit the way `wlt-net`/`wlt-filemanager` were.
    `net_engine.c`, `anomaly_engine.c`, and `forensic_log.c` already had
    specific point-fixes from 2026-08-04 (gap #29) but haven't had a
    full read at this rigor. Remaining: all 16 source files above (most
    not started or only point-fixed), the rest of `bpf/guardian.bpf.c`,
    and `mcp_server.py`.
43. **`policy_engine.c` full read + fix, 2026-08-16**: `default_action()`'s
    critical-path check used `strstr()` (substring anywhere in
    `ev->path`) instead of a prefix check - "/etc/passwd" as a search
    term would also match an unrelated file like
    "/mnt/backup/etc/passwd". False-positive direction (over-triggers
    RESTORE_FILE on unrelated paths), not a missed detection. Fixed to
    anchor at the start of the path via `strncmp`. Also verified, not
    assumed: the config-file parser's `sscanf` field widths
    (`%31s`/`%63s`/`%15s`/`%3s`/`%31s`) against the actual header
    constants (`G_SOURCE_MAX=32`, `G_RULE_ID_MAX=64`) - exactly matched,
    no overflow.
44. **`action_engine.c` full read + severe fix, 2026-08-16**: the
    human-confirm gate for irreversible actions (KILL/DELETE/
    KILL_SWITCH) compared `getenv("GUARDIAN_CONFIRM_TOKEN")` against
    `g_confirm_token`, a function-local static initialized to `{0}`
    (empty string). `strcmp("", "") == 0`, so
    `GUARDIAN_CONFIRM_TOKEN=` (set to empty) bypassed confirmation
    entirely. Fixed by explicitly rejecting an empty stored token
    regardless of the env var's content - strictly more restrictive,
    can't block a real non-empty token. This function's own comment
    describes the intended flow as "set via the API socket by the user
    (kairos confirm <token>)" - see #45 for whether that path actually
    exists anywhere.
45. **`kairos_cli.c` full read - resolves #44's open question, and a
    separate severe finding, 2026-08-16**: no `confirm` subcommand
    exists here - only investigate/explain/status are recognized in
    `main()`. The `g_confirm_token` setter described in action_engine.c's
    comment is not in this file (checked #46 next - not there either).
    Separately, and far more severe: `call_model()` built a `curl` POST
    command via `popen()`, interpolating the JSON request body - built
    from `event_id`/`question`, i.e. raw, completely unsanitized
    `argv[2]` - directly into a single-quoted shell string (`-d '%s'`).
    The JSON-escaping in this same function only handles `"`, `\`, and
    `\n` - it does not touch shell metacharacters, especially the single
    quote. A single quote in either CLI argument broke out of the shell
    string and ran anything after it as a literal command - e.g.
    `kairos explain "foo' ; touch /tmp/pwned ; echo '"` executes touch,
    as whoever runs this CLI (a security-investigation tool, plausibly
    privileged). **This is the most severe finding of the whole
    session** - full command injection, not a logic/policy bug. Fixed by
    writing the body to a securely-created temp file (`mkstemp`) and
    having curl read it via `--data-binary @path` - the shell command
    string now contains only a program-generated path, never anything
    user-supplied.
46. **`mcp_server.c` - confirm-mechanism resolved, plus a distinct auth
    bug across 5 call sites, 2026-08-16 (targeted read of the
    irreversible-tool section only, ~350 of 1013 lines - not a full
    audit)**: this file does not use `action_engine.c`'s
    `g_confirm_token` at all - it has its own, separate, older-style
    gate: `!getenv("MCP_AUTOCONFIRM")`, checked at the `tools.call` tier
    gate plus redundantly in each of `t_process_kill`/`t_secure_delete`/
    `t_kill_switch`/`t_file_encrypt`. Since this file is the actual
    MCP/AI-agent-facing interface (what Kairos itself would call), this
    is arguably the more consequential of the two mechanisms, not a
    secondary one. The bug: `getenv()` returns non-NULL for ANY value,
    so `MCP_AUTOCONFIRM=0` or `=false` bypassed the gate identically to
    `=1`, despite every error message in this file documenting "=1" as
    the required value - someone trying to explicitly disable
    auto-confirm by setting it to 0 would accidentally enable it. Fixed
    with one helper (`mcp_autoconfirmed()`, checks `strcmp(v,"1")==0`)
    applied at all 5 live call sites. Two DISABLED tools intentionally
    left unfixed: `t_shell_exec` (takes a fully attacker-controlled
    `cmd` straight into `popen()` - an even more direct injection
    primitive than #45) and `t_install_package` are commented out of the
    `TOOLS[]` registry with an explicit "C1 security fix... MUST NOT be
    callable via the MCP protocol" comment - verified this is
    structurally real (`N_TOOLS` is computed from the actual compiled
    array size, so these two are genuinely unreachable via `tools.call`),
    not just an unenforced comment. Good prior mitigation, correctly
    credited here - but the vulnerable functions still exist in source;
    if anyone ever re-enables those two registry lines without also
    fixing the underlying `popen()` calls, the vulnerability returns
    immediately. **Guardian coverage after this session:** `guardian.c`,
    `policy_engine.c`, `action_engine.c`, `kairos_cli.c` full reads;
    `mcp_server.c` partial (irreversible-tool section + tier gate only)
    + one eBPF function pair. Remaining, not started: `anomaly_engine.c`,
    `canary_engine.c`, `ebpf_engine.c`, `fim_engine.c`, `forensic_log.c`,
    `iforest.c`, `net_engine.c`, `pmu_engine.c`, `ransomware_engine.c`,
    `sig_engine.c`, `watchdog.c`, the rest of `mcp_server.c` (~660
    lines), the rest of `bpf/guardian.bpf.c`, and `mcp_server.py`.
47. **`net_engine.c` full read + fix, plus an open architectural
    question, 2026-08-16**: verified the 2026-08-04 parser fix (gap #23)
    genuinely holds up under a full column-by-column trace against the
    real `/proc/net/tcp` format, not just trusted the commit message.
    Found: `find_tor()` runs exactly once, at `g_net_init()`. If Guardian
    starts before Tor is actually up - plausible, Tor may boot in a
    separate VM/partition with its own sequencing - `tor_uid` gets stuck
    at the "not found" sentinel permanently, and this engine flags ALL
    non-root traffic as suspicious forever, even after Tor successfully
    starts later. Fixed with a retry in `g_net_check_leak()` that only
    fires while the UID is still unresolved. **Open question, not
    resolved**: this whole engine detects leaks by matching a
    `/proc/net/tcp`-visible connection's UID against Tor's UID, which
    presupposes Tor runs in the same process namespace as this code.
    Given the project's now-proven TorPD architecture runs Tor in a
    separate VM, it's unclear whether this code path ever actually sees
    a `tor` process in its own `/proc` at all, or whether it's
    pre-VM-split logic that `ebpf_engine.c` (not yet read) or the seL4
    partition boundary itself has since superseded. Needs confirming
    which VM this actually runs in before trusting its coverage.
48. **`fim_engine.c` full read - clean, 2026-08-16**: no new issues
    found. Genuinely careful: GCM encryption with the path bound as AAD
    (tamper of either the ciphertext or a swapped path both fail auth),
    `RAND_bytes`-only IV generation that explicitly refuses to encrypt
    rather than fall back to a weaker source, inotify queue-overflow
    handling that triggers a full rescan, and FIM restore delegates to a
    single authoritative implementation (`g_action_restore_file`) rather
    than duplicating one. One acknowledged, low-severity limitation: the
    64-directory inotify watch-map is a documented intentional cap: past
    it, events degrade to a bare filename instead of a full path rather
    than crashing or being dropped silently.
49. **`ransomware_engine.c` full read + severe fix, 2026-08-16**:
    verified the rate-vs-cumulative delta/per-minute math itself (gap
    #4's own fix, and what `guardian.c`'s self-test specifically
    exercises) is correct. But `find_slot()`'s fixed 64-slot
    prev-counter table had no eviction: `re->n` only ever grew, with no
    reclamation for PIDs whose process has since exited. On any
    normally-active system, 64 distinct PIDs cycle through in minutes -
    after that, `find_slot()` returned -1 forever for any new PID, and
    the main scan loop silently skipped it (`if (si < 0) continue`).
    Meaning: the entire ransomware engine stopped evaluating *any new
    process* - including one that just started doing the exact
    bulk-delete/rename-storm/high-write pattern this file exists to
    catch - shortly after Guardian starts, on any real system. Fixed
    with dead-PID slot reclamation (`kill(pid,0)`/`ESRCH`, the standard
    no-signal existence check) when the table is full. Secondary,
    lower-priority, not fixed: `find_slot()` matches purely on PID
    value, so if a PID is reused by an unrelated new process *before*
    the table fills (so the eviction path above never triggers), that
    new process's first check-interval activity gets compared against
    the old process's stale counters and may read as a 0 delta -
    self-corrects on the next cycle, so lower stakes than the slot
    exhaustion above; a full fix would need `/proc/<pid>/stat`'s
    starttime field to detect PID reuse, not attempted here.
50. **`ebpf_engine.c` full read - no new severe bug in this file itself,
    but confirms a real cross-file gap in #49, 2026-08-16**: this file is
    genuinely mature - multi-program `.bpf.o` link tracking (a scalar
    field previously dropped every attach after the first, leaking each
    `bpf_link` for the daemon's lifetime and losing the ability to manage
    22 of guardian.bpf.c's 23 tracepoints), a ring buffer created once
    and reused instead of the old per-poll create/destroy churn, a
    rule-condition parser where an unterminated `in (...)` list used to
    infinite-loop (fixed) and a malformed rule opening with "or" used to
    match unconditionally before evaluating any real predicate (fixed,
    with a `have_evaluated` guard), and a well-reasoned, explicitly
    revisited architecture decision documented in `g_ebpf_set_kill_
    switch_active()` about why the kill switch is user-reversible rather
    than one-way given the current (no seL4 partition isolation yet)
    privilege model. Confirms: `g_ebpf_get_map_value()` returns
    `-ENOSYS`/`-ENOENT` on failure, and `ransomware_engine.c` (#49) never
    checks that return value - its counters silently stay zero-
    initialized if eBPF is unavailable, with no equivalent to this file's
    own clear "[ebpf] BPF load/attach unavailable... degraded mode"
    startup notice. Not fixed here - the higher-level degraded-mode
    signal already exists at the `ebpf_engine.c` level, so this is a
    secondary visibility gap, not a silent total blind spot with zero
    signal anywhere. Lower-confidence, not verified: whether kernel-side
    `bpf/guardian.bpf.c` guarantees `be->path`/`be->comm` are always
    null-terminated within their 128/16-byte wire format before
    `g_strlcpy` reads them in `ringbuf_cb()` - would need to check both
    the kernel-side write logic and `g_strlcpy`'s own implementation
    before calling this a real bug either way.
51. **`anomaly_engine.c` full read - no new bug, one confirmed positive
    contrast, 2026-08-16**: `call_python_model()`'s subprocess invocation
    uses `fork()`+`execvp()` with a real argv array
    (`{ae->py_bin, script, "--check", args, NULL}`), not
    `popen()`/`system()` - no shell is ever invoked, so nothing in `args`
    (which is only ever `%.2f`-formatted numeric feature values anyway,
    never a raw string) could be interpreted as shell syntax even in
    principle. This is the pattern #45's fix moved `kairos_cli.c` to -
    good confirmation it already existed correctly elsewhere in this
    codebase, making #45 a genuine isolated oversight rather than a
    systemic blind spot. Also includes `PR_SET_PDEATHSIG` so the child
    can't outlive a killed parent. One minor, low-severity oddity, not
    fixed: `verify_model_hash()`'s fallback when `<model_path>.sha256` is
    missing checks a hardcoded `ml/anomaly.pkl.sha256` regardless of what
    `model_path` actually is - confusing if the real model is ever named
    something else, but a mismatch still fails closed (returns -EACCES,
    `g_anomaly_init` returns NULL), so this is a robustness/clarity issue
    rather than a bypass.
52. **External research pass against real open-source security tools,
    2026-08-22 - two concrete fixes it produced**: read `sig_engine.c` in
    full first (not previously audited) - real YARA (`libyara`), real
    Ed25519-signed updates fetched over Tor with `--socks5-hostname`
    (correct flag for .onion resolution). Then cloned Falco, YARA, and
    ClamAV into a separate sandbox to read actual production source, not
    summaries of it, targeted at Guardian's own weak points:
    - **`canary_engine.c` (ransomware bait files, unrelated to
      sig_engine.c's canary despite the shared name - confirmed by
      reading it, not assumed) had two real weaknesses against documented
      EDR-canary-evasion technique**: the filename was a single fixed
      string (`!GUARDIAN_CANARY_DO_NOT_TOUCH.bin`), identical across
      every installation and self-announcing - real ransomware families
      are documented checking for and skipping known EDR canary name
      patterns, and one hardcoded exclusion would have defeated every
      installation at once. Placement was also system directories
      (`/tmp`, `/var/tmp`, `/etc`, `/root`, `/home`) rather than where a
      desktop ransomware attack actually encrypts - a user's real
      Documents/Pictures/Desktop/Downloads. Fixed: filename is now
      derived deterministically per-machine from `/etc/machine-id`
      (stable across restarts on the same box so the existing
      O_EXCL-retry/re-hash logic still works, different per
      installation so no single exclusion works everywhere), styled to
      look like a real user file (photo/scan/document naming), and
      placement now includes the real per-user target directories for
      every user found under `/home`. `G_MAX_CANARIES` raised 8->24 to
      fit the larger, more realistic set.
    - **`sig_engine.c`'s YARA-update canary had a real ordering bug,
      confirmed against guardian.c's actual canary loop** (a *different*,
      statistical canary from the one above - 300s of post-swap alert-rate
      monitoring in `guardian.c`'s main loop, `g_canary_active`/
      `g_canary_start_time`): `g_sig_fetch_updates()` did the atomic
      `rename()` onto the live on-disk rules path immediately after
      signature verification - before the 5-minute canary window even
      started. A daemon restart anywhere in that window (a long window
      for an unrelated crash/update/reboot to land in) meant
      `g_sig_init()`'s next boot loaded the new, never-validated rules
      directly, with zero record a canary was ever pending. Compared
      directly against ClamAV's freshclam+clamd: freshclam verifies fully
      in a temp directory before ever renaming onto a live path, and
      separately, clamd builds an entirely new engine object, fully loads
      and validates it, and only swaps the live pointer on success -
      confirmed via `clamd/server-th.c`'s actual reload-failure logging
      ("keeping the previous instance"). Restructured `sig_engine.c` to
      match: `g_sig_fetch_updates()` now stages verified rules and swaps
      only the in-memory pointer (so the canary period genuinely exercises
      the real new rules against real traffic); `g_sig_commit_updates()`
      is now the only place the on-disk rename happens, reached only
      after the canary passes; `g_sig_rollback_updates()` now just
      discards the staged file, since the on-disk original was never
      touched. Also removed `copy_file()`, now fully unused after the
      restructure - would have failed the `-Werror` build as an unused
      static function; caught by grepping for remaining call sites before
      moving on, not assumed safe to delete.
    None of this is "superior to every AV on the internet combined" -
    that's not a real target, full stop - but both fixes are concrete,
    evidence-grounded improvements over what shipped before, each
    directly informed by reading real production code rather than
    guessing at best practice.

## 6. Phase/workstream framing that's still active

**Update, 2026-08-16**: superseded twice since the paragraph below was
written. `docs/context/PHASE_BOOT_FIX_AND_TORPD_VERIFY.md` (2026-08-02)
covered the UEFI fix, Firewall PD reproducibility, and TorPD verification
- all three sections now complete per gaps #27-30 above. PHASE MUSL-TOR
(handed to GLM 2026-08-16, not yet a committed file) covers finishing the
TorPD SOCKS5 end-to-end test per gap #28. Original paragraph below is
2026-08-02-and-earlier context, kept for the history.

`docs/context/PHASE_SEL4_FINISH_AND_POLISH_ARCHIVED_2026-07-29.md`
(2026-07-28, i.e. written the same day as several of the commits in §1's
table) laid out 4 workstreams - seL4 vm1+Ed25519 finish, desktop UX,
package hosting, and a fourth not enumerated in the doc excerpt reviewed -
in checklist format. Given §1's table shows real completions against at
least 3 of those workstreams within the same day/next few days, this phase
doc's checklist is a reasonable **current** source for "what's being worked
on right now" - re-read it directly (it's archived, not deleted) rather
than relying solely on this summary if planning the next session's work.

53. **Resolved gap #47's open architectural question, by reading the
    actual proven CAmkES assembly rather than inferring - good news,
    2026-08-22**: `src/sel4-partitions/tor-pd/camkes/app/minimal_tor_pd.camkes`
    (the reproducibility-fixed version from `96a5c13`, matching the
    topology gap #27/ADR-09 actually proved works) shows vm0 has **no
    network hardware assigned to it at all** - no `pci_devices`, no
    `vm_ioports`, no `vm_irqs` for networking anywhere in vm0's config.
    Only vm1 has the e1000 NIC PCI passthrough (`vm1.pci_devices`,
    `vm1.vm_ioports`, `vm1.vm_irqs`). The only connections between vm0
    and vm1 in this assembly are the two 4KB shared-memory dataports and
    two notification channels that are the ADR-09 IPC bridge itself -
    nothing else. Compared directly against Whonix's own documented
    architecture (confirmed via web search, not assumed): "Whonix-
    Workstation is on a completely isolated network. Only connections
    through Tor are possible" and "not even malware with root privileges
    can find out the user's real IP." vm0 having zero network hardware is
    the same class of guarantee, enforced by seL4 capability-based
    hardware assignment rather than a hypervisor-level virtual switch -
    arguably a stronger enforcement mechanism than Whonix's own. This
    directly answers gap #47's question about `net_engine.c`: Tor almost
    certainly cannot run in the same VM as `net_engine.c` if that engine
    runs in vm0 (which its plain `/proc/net/tcp` + iptables/wpa_supplicant
    style strongly suggests), meaning its whole approach - scanning its
    own `/proc` for a local `tor` process by UID - is very likely checking
    for something that architecturally cannot exist there. It would need
    to check something derived from the vm0<->vm1 bridge state instead
    (e.g. the tor_ready/tor_done notification channel) rather than a local
    process. **Important caveat, not yet resolved**: this file is named
    "minimal_tor_pd.camkes" and its header says it's "based on the proven
    x86 multi-VM pattern from optiplex9020.camkes" - reads like a focused
    proof-of-concept built specifically to prove the IPC bridge, not
    necessarily the final production assembly. Does the shipped desktop
    build add its own network device to vm0 for things like the WiFi
    scanning/DHCP config `wlt-net` implements (`wifi.c`/`ethernet.c`),
    which would reintroduce the softer isolation model? Needs confirming
    which assembly file the actual shipped build uses before treating
    vm0's hard isolation as settled for the full system, not just this
    proof-of-concept.

54. **wlt-crypto started: keyvault.c clean, luks.c full read + one real
    fix, 2026-08-23**: `keyvault.c` is honest, well-designed stub/bridge
    code - the not-yet-implemented Microkit IPC path returns
    `WLT_CRYPTO_ERR_UNSUPPORTED` rather than faking success, and the
    working bridge-client function (`wlt_keyvault_get_volume_key_fd`)
    fails closed if the bridge daemon isn't running, with correct
    `volatile`-pointer zeroing of key material after use. No findings.
    `luks.c` is exceptionally careful - real `mkstemp`+`O_EXCL`+
    `fchmod(0600)`+`FD_CLOEXEC` for the keyfile, `O_NOFOLLOW` on cleanup
    to defeat a symlink swap between write and remove, and an honestly
    -refused `kexec` RAM-wipe stub that documents precisely why the
    *previous* implementation was structurally broken (`nr_segments=0`
    is the kexec unload opcode, not load, and `reboot()` was never
    called) rather than shipping something that fakes a secure wipe.
    One real gap, fixed: `remove_keyfile()`'s entire security value
    (zero+ftruncate+fsync+unlink instead of a plain unlink) depends on
    `/run/wlt` actually being tmpfs - on real storage, overwriting a
    file before deleting it doesn't reliably erase the original LUKS
    volume-key bytes (journaling may have logged the original write;
    SSD wear-leveling/copy-on-write FTL behavior commonly routes an
    overwrite to different physical NAND than the first write). This was
    assumed, never checked. Added a `statfs()`-based tmpfs verification
    before trusting the directory with key material - fails closed, no
    behavior change on any correctly-configured system since `/run` is
    tmpfs virtually everywhere. Remaining in `wlt-crypto`, not started:
    `crypto.c`, `file_crypto.c`, `secure_delete.c`.

55. **`crypto.c` full read + severe fix - the entire real LUKS
    disk-encryption path was unreachable at the public API level,
    2026-08-23**: `wlt_crypto_luks_format()` and `wlt_crypto_luks_open()`
    both gated on `g_crypto_initialized`, which only becomes true via
    `wlt_crypto_init()` -> `wlt_keyvault_init()` - the OLD Microkit-
    direct-IPC stub in `keyvault.c` that unconditionally returns
    `WLT_CRYPTO_ERR_UNSUPPORTED`. It can never succeed, so
    `g_crypto_initialized` can never become true, so these two functions
    - the ones that actually drive the WORKING bridge-based LUKS path
    through `wlt_keyvault_get_volume_key_fd()` and `luks.c` (both
    reviewed clean in #54) - were permanently unreachable regardless of
    whether the KeyVaultPD bridge daemon is running correctly. Telling
    detail that confirms this wasn't intentional: `luks_close()` and
    `luks_status()` right below don't have this gate at all - only the
    two functions that actually need bridge availability do, and the
    gate they got checks the wrong thing. This means real disk
    encryption - one of the headline "Tails-like real security" claims
    for this whole OS - has been structurally dead at the dispatcher
    level this entire time, independent of whether the bridge daemon or
    KeyVaultPD themselves work. Fixed by removing the dead gate from
    both functions; each already has its own real, working, fail-closed
    check a few lines later (`wlt_keyvault_get_volume_key_fd()` itself
    fails closed if the bridge isn't up), matching how `luks_close()`/
    `luks_status()` were already correctly written. Left the same gate
    in place on `wlt_crypto_encrypt`/`decrypt`/`encrypt_file`/
    `decrypt_file`, since those route through `wlt_keyvault_encrypt`/
    `decrypt`, which are themselves still genuine stubs (unlike LUKS,
    there's no working path being blocked there - removing the gate
    wouldn't unlock anything real). Also confirmed real, well-designed
    hardening already in place here: device paths scoped to `/dev/` with
    `..` rejected (closes writing LUKS format over an arbitrary file
    like `/etc/passwd`), LUKS names allowlisted to `[A-Za-z0-9._-]`
    (closes shell-metacharacter-via-cryptsetup-args), and buffer-overflow
    -safe size checks on encrypt/decrypt. Remaining in `wlt-crypto`, not
    started: `file_crypto.c`, `secure_delete.c`.

56. **`file_crypto.c` and `secure_delete.c` full reads - both clean,
    completes `wlt-crypto` (all 5 files), 2026-08-23**: `file_crypto.c`'s
    streaming encrypt/decrypt is genuinely careful - crash-safe atomic
    rename (fsync of the new content strictly before rename, with a
    comment explaining why: skipping that risks losing both plaintext
    and ciphertext on power loss between rename and durability),
    post-decrypt buffer-overrun validation against a tampered header,
    and correct key-ID mismatch detection between caller and file
    header. It calls through `wlt_keyvault_encrypt`/`decrypt`, still
    genuine stubs per #54 - unlike the LUKS case in #55, there's no
    working path being incorrectly blocked here, just honest
    incompleteness, not a wiring bug. One lower-confidence, lower-
    priority note, not fixed: `open_input_file()` doesn't use
    `O_NOFOLLOW`, so a symlink race at `path` between caller-decision and
    open could target an unintended file - worth having eventually, but
    narrower and more speculative than #55, and this file's callers
    aren't obviously attacker-facing. `secure_delete.c` is genuinely
    clean - no findings. Notably, it already correctly documents in its
    own header the exact SSD/copy-on-write remapping limitation of
    overwrite-based shredding ("not a strong guarantee... for full-disk
    confidentiality, rely on LUKS"), already has the same
    O_NOFOLLOW-after-`lstat()` TOCTOU close this session added elsewhere,
    and its pattern-cycling (`pass_index % 3`) correctly rotates random/
    zero/ones for any pass count - unlike `wlt-filemanager`'s own
    secure-delete (audited earlier this session), which just repeats
    0xFF forever past pass 2. **`wlt-crypto` is now fully audited (all 5
    files: crypto.c, keyvault.c, luks.c, file_crypto.c, secure_delete.c)
    - headline result is #55's fix, everything else here was
    confirmation of already-solid work.**

57. **`wlt-permissions` started: `permissions.c`, `manifest.c`,
    `persist.c` full reads - all three clean, two precise hypotheses
    checked and ruled out, 2026-08-23**: `wlt_perms_decision_of()` is
    the actual access-control core (maps a stored value to allow/ask/
    deny) - checked whether its per-kind value space
    (NETWORK/FILESYSTEM/TRI/BOOL) could numerically overlap across
    kinds, which would be a real type-confusion risk. It can't:
    `wlt_perms_value_t` deliberately gaps its ranges (0-2 shared by
    tri/bool, 10-11 network, 20-22 filesystem) specifically to prevent
    this, per its own header comment. Also checked whether
    `wlt_perms_register()` - which copies `manifest->values[]` straight
    into the live grant table with no call to `wlt_perms_value_is_legal()`,
    unlike `wlt_perms_grant()` which always validates - could let an
    illegal cross-kind value in via a manifest. It can't either:
    `wlt_perms_value_from_str()` is itself kind-aware (rejects any
    string that doesn't map to a legal value for the given kind), so
    it's structurally impossible for the parser to ever produce an
    illegal value in the first place, regardless of whether the separate
    `wlt_perms_manifest_validate()` legality loop runs afterward - that
    loop is confirming something already true by construction, not the
    only safeguard. `persist.c`'s grant-file loader uses this same
    kind-aware parser and separately re-checks `declared_mask` before
    applying any loaded grant, so a tampered/stale grant file can't
    grant an undeclared permission or an illegal value either. One
    minor, low-priority, not-fixed note: `manifest.c`'s `read_next_line()`
    silently truncates and desyncs on any line over 511 bytes - the
    unconsumed remainder gets reparsed as a bogus new line next
    iteration. Low real-world likelihood (manifest lines are short
    key=value pairs) and fails toward a warning/skip, not a bypass.
    Remaining in `wlt-permissions`, not started: `audit.c`, `badges.c`.

58. **`audit.c` and `badges.c` full reads - both clean, completes
    `wlt-permissions` (all 5 files), 2026-08-24**: `audit.c`'s
    log-injection defense was verified, not just read as claimed -
    `sanitise()` strips `\n`/`\r`/`\t` from app_id/by/detail before
    formatting, closing the classic forged-log-line attack (an app
    named e.g. `foo\n<fake timestamp> action=grant by=root` could
    otherwise inject an apparently-separate, fabricated audit entry).
    Also verified, not trusted: the header's PIPE_BUF-atomicity claim -
    the actual `write()` call is provably capped at 511 bytes (explicit
    clamp before the write), comfortably under the 4096-byte guarantee,
    and `O_APPEND` makes that write atomic across concurrent processes
    per POSIX. `badges.c` was traced line by line against its own
    documented RED/YELLOW/GREEN policy (RED: full-fs, cam+mic pair, or
    background+autostart pair; YELLOW: direct-net, home:rw, clipboard,
    or any single sensitive sensor; GREEN: everything else) - matches
    exactly, correct worst-case-first precedence. **`wlt-permissions` is
    now fully audited (all 5 files: permissions.c, manifest.c,
    persist.c, audit.c, badges.c) - genuinely solid module. Two
    precise hypotheses raised and ruled out in #57, zero severe findings
    across all 5 files - a real contrast with #55's crypto.c gating bug
    and Guardian's several severe findings, worth knowing as a
    calibration point, not every module in this codebase is at the same
    risk level.**

59. **`wlt-filemanager/wlt_fm_safe/` - new, memory-safe Rust
    reimplementation of gap #37's symlink-recursion fix, execution-
    verified (not just read), 2026-08-28**: written in response to
    explicit direction to build/produce rather than continue read-only
    auditing. Two functions with a C-compatible FFI
    (`wlt_fm_safe_copy_directory`/`wlt_fm_safe_delete_directory`)
    reimplementing the same operations `operations.c` already has, using
    Rust's `symlink_metadata` (non-following) vs `metadata` (following)
    API split to make "did I mean to follow this symlink" a conscious
    choice at every call site by construction, rather than the kind of
    easy-to-miss `stat()`/`lstat()` swap that caused gap #37 in the first
    place - there is one recursive function per operation here, not the
    two hand-synced guards (entry-point + per-entry loop) the C fix
    needed. 9 unit tests, including the exact exploit scenario from #37
    (a directory containing a symlink to an external target - verifies
    the target survives a delete of the containing folder), a top-level-
    symlink-as-argument case, two symlink-cycle-termination cases (would
    hang if the safety property were broken), an ordinary-tree regression
    sanity check, and a permission-preservation check. Verified by real
    execution twice: once in the authoring sandbox (`cargo test`, 9/9
    pass, plus a real standalone C program compiled and linked against
    the built `.so`, confirming the FFI layer works from actual C code,
    not just in principle), and a second time by pulling the exact bytes
    that landed in this repo back out into a fresh sandbox copy and
    re-running the full suite against them, to confirm the chunked file
    transfer itself didn't corrupt anything rather than assuming it from
    individual write calls succeeding (one did time out mid-transfer;
    confirmed via file size/line count exactly where it had actually
    stopped before resuming, rather than guessing). **NOT yet compiled in
    this project's real target toolchain (WSL2/QEMU) - rustc/cargo
    availability there is unconfirmed - and NOT wired into
    `wlt-filemanager`'s Makefile or called from `operations.c` yet.**
    Both are deliberate: forcing a new toolchain dependency into the real
    build, or swapping proven-in-a-different-environment logic into
    production, without confirming either first, would be the same kind
    of blind push this document has flagged against all session. Full
    integration steps are in `wlt_fm_safe/README.md`. This does not
    replace the existing C fix from gap #37, which remains the live,
    already-integrated code - this is a demonstrated-safer alternative
    ready for a build-and-verify pass, not a completed migration.

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
