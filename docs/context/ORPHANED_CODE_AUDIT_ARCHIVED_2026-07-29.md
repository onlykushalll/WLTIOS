# Orphaned Code Audit — 2026-07-16 (line counts corrected 2026-07-18)

> **Method:** Each .c file compiled standalone (`gcc -c`), grep for stubs/TODOs,
> deep-dive on files with `main()`. No assumptions from docs or comments.
>
> **Correction note (2026-07-18):** The line counts in the original version of
> this doc were wrong for every directory — inflated relative to `Get-Content
> file | Measure-Object -Line` run directly against the tracked files at commit
> `93f11b0`. Numbers below are the direct measurement; verify against the same
> commit if this doc is read after further changes. The qualitative
> stub/compiles findings from the original audit were re-checked and stand.

## Summary Table (corrected)

| Directory | Files | Lines (verified) | Lines (original doc, wrong) | Compiles Clean | Has main() | Recommended Action |
|-----------|-------|-------------------|------------------------------|-----------------|------------|---------------------|
| **wlt-ai/** | 19 | **5,114** | ~5,700 | 15/19 clean | kairos_window.c only | Wire ai.c+tools.c+memory.c into kairos_window (needs libcurl or a curl-free rewrite) |
| **wlt-permissions/** | 5 | **1,246** | 1,390 | 5/5 clean | None | Wire as library — cleanest code of the batch, zero stubs |
| **wlt-tor-mgr/** | 7 | **2,060** | 2,246 | 6/7 clean | tor_mgr.c | Real Tor control-port code — built as `wlttor`, but see reproducibility note below |
| **wlt-pkg/** | 6 | **827** | 984 | 6/6 clean | None | Audit only — heavily stubbed despite clean compile |
| **wlt-update/** | 4 | **1,525** | 1,667 | 4/4 clean | None | Audit only — needs libsodium for Ed25519 |
| **TOTAL** | 41 | **10,772** | ~7,944 (sum of original rows) | — | — | — |

## Per-File Detail (verified line counts)

### wlt-ai/ (19 files, 5,114 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| ai.c | 778 | needs curl/curl.h | None found | **REAL** — core dispatch, HTTP chat, tool parsing. Needs libcurl. |
| tools.c | 451 | OK | None found | **REAL** — 15+ tool implementations |
| memory.c | 352 | OK | None found | **REAL** — conversation history, file-backed persistence |
| config.c | 441 | OK | None | **REAL** — config parsing |
| kairos_mcp.c | 405 | OK | None | **REAL** — MCP protocol bridge |
| vision.c | 497 | needs curl/curl.h | None | **REAL** — image analysis, needs libcurl |
| guardian.c | 286 | needs fcntl fix | 9 TODOs | **STUB HEAVY** — monitoring hooks all TODO |
| privacy_pipeline.c | 228 | OK | None | Likely real |
| sidebar.c | 158 | OK | None | Likely real (library, no main) |
| file_search.c | 149 | OK | None | **REAL** — directory search |
| privacy_analysis.c | 147 | OK | None | Likely real |
| cloud_api.c | 151 | OK | None | Likely real |
| voice.c | 160 | OK | None | Likely real |
| local_model.c | 122 | OK | None | Likely real |
| hotword.c | 122 | OK | None | Likely real |
| proactive_protection.c | 143 | OK | None | Likely real |
| anonymity_coach.c | 109 | OK | None | Likely real |
| visual_feedback.c | 95 | OK | None | Likely real |
| kairos_window.c | 320 | needs xdg-shell header | None | **REAL** — built this session |

### wlt-permissions/ (5 files, 1,246 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| manifest.c | 402 | OK | None | **REAL** — manifest parser |
| permissions.c | 307 | OK | None | **REAL** — permission grant/deny logic |
| persist.c | 292 | OK | None | **REAL** — file-backed persistence |
| audit.c | 148 | OK | None | **REAL** — audit log |
| badges.c | 97 | OK | None | **REAL** — privacy badge system |

**Verdict unchanged from original audit:** cleanest orphaned code, zero stubs, ready to wire into wlt-sandbox/launch.c.

### wlt-tor-mgr/ (7 files, 2,060 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| tor_mgr.c | 651 | OK | None | **REAL** — has main(), CLI dispatch |
| control.c | 426 | OK | None | **REAL** — Tor control port protocol |
| circuits.c | 276 | OK | None | **REAL** — parses real circuit-status |
| config.c | 247 | OK | None | **REAL** |
| bridges.c | 217 | OK | None | **REAL** — bridge configuration |
| identity.c | 92 | OK | None | **REAL** — sends NEWNYM via control port |
| tor_vm.c | 151 | needs fcntl | 2 TODOs (seL4 VMM) | Expected — seL4 VMM not built yet |

**Reproducibility note (added 2026-07-18):** `wlttor` compiling and Tor
bootstrapping in QEMU was verified real on 2026-07-16, but the fix depended on
manual `cp`/symlink operations into the live rootfs staging directory that
were never captured in a script. As of commit `93f11b0`, this has been
partially fixed — liblzma/libzstd/libevent and the Tor binary itself are now
vendored under `packages/tor-libs/` and referenced by `build-initramfs64.sh`,
and a clean-rebuild test confirmed Tor bootstraps again. **Not yet confirmed:**
whether the compositor/terminal's own dependencies (`libwlroots`,
`libwayland-client`) survive the same clean-rebuild test — last report said
they do not, and that gap is still open as of this writing.

### wlt-pkg/ (6 files, 827 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| pkg.c | 214 | OK | 5 TODOs (install/remove/update flows) | **STUB** |
| download.c | 192 | OK | None | Likely real |
| verify.c | 143 | OK | None | Likely real |
| transaction.c | 112 | OK | None | Likely real |
| store.c | 105 | OK | None | Likely real |
| dependency.c | 61 | OK | 4 TODOs + `g_db_count_hack` returns 0 | **STUB** |

**Verdict unchanged:** core install logic never written. Audit only.

### wlt-update/ (4 files, 1,525 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| update.c | 653 | OK | None found | Likely real |
| partition.c | 405 | OK | None found | Likely real |
| config.c | 259 | OK | None found | Likely real |
| verify.c | 208 | OK | None found | Needs libsodium |

**Verdict unchanged:** clean compile, needs libsodium for Ed25519. Lower
priority than A-C. Audit only for now.

## Key findings (unchanged from original — qualitative claims re-verified, only line counts were wrong)

**circuits.c is REAL, not placeholder.** Sends `GETINFO circuit-status` to
Tor's control port, parses circuit structs with relay fingerprints, resolves
node info via `GETINFO ns/id/<fp>`, does country lookup via
`GETINFO ip-to-country/<ip>`. identity.c sends real `SIGNAL NEWNYM`.

**ai.c is REAL** (778 lines, not 921 as originally stated) but needs libcurl.
Builds OpenAI-compatible chat JSON, POSTs to llama-server at 127.0.0.1:8080,
parses tool_calls. tools.c (451 lines) has 15+ real tool implementations.
memory.c (352 lines) has real conversation persistence. As of 2026-07-18 this
is still sitting in the tracked repo unmodified (`git diff HEAD` on these three
files is empty) — a prior session's claim of a working `libwlt-ai.a` was built
against a hand-patched scratch copy in WSL2 that was never committed back.
