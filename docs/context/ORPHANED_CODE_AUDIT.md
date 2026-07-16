# Orphaned Code Audit — 2026-07-16

> **Method:** Each .c file compiled standalone (`gcc -c`), grep for stubs/TODOs,
> deep-dive on files with `main()`. No assumptions from docs or comments.

## Summary Table

| Directory | Files | Lines | Compiles Clean | Stubs/TODOs | Has main() | Recommended Action |
|-----------|-------|-------|----------------|-------------|------------|-------------------|
| **wlt-ai/** | 19 | ~5,700 | 15/19 clean | guardian.c has 9 TODOs (monitoring hooks) | kairos_window.c only | Wire ai.c+tools.c+memory.c into kairos_window (needs libcurl) |
| **wlt-permissions/** | 5 | 1,390 | 5/5 clean | None found | None | Wire as library — cleanest code of the batch |
| **wlt-tor-mgr/** | 7 | 2,246 | 6/7 clean | tor_vm.c has 2 TODOs (seL4 VMM, expected) | tor_mgr.c | **BUILD IT** — real Tor control port code, not placeholder |
| **wlt-pkg/** | 6 | 984 | 6/6 clean | 10+ TODOs (dependency graph, install flow) | None | Audit only — heavily stubbed despite clean compile |
| **wlt-update/** | 4 | 1,667 | 4/4 clean | None found, needs libsodium | None | Audit only — needs libsodium for Ed25519 |

## Per-File Detail

### wlt-ai/ (19 files, ~5,700 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| ai.c | 921 | ❌ needs curl/curl.h | None found | **REAL** — core dispatch, HTTP chat, tool parsing. Needs libcurl. |
| tools.c | 524 | ✅ | None found | **REAL** — 15+ tool implementations (file search, system info, network) |
| memory.c | 422 | ✅ | None found | **REAL** — conversation history, file-backed persistence |
| config.c | 522 | ✅ | None | **REAL** — config parsing |
| kairos_mcp.c | 484 | ✅ | None | **REAL** — MCP protocol bridge |
| vision.c | 579 | ❌ needs curl/curl.h | None | **REAL** — image analysis, needs libcurl |
| guardian.c | 342 | ❌ needs fcntl fix | 9 TODOs | **STUB HEAVY** — monitoring hooks all TODO |
| privacy_pipeline.c | 261 | ✅ | None | Likely real |
| sidebar.c | 183 | ✅ | None | Likely real (library, no main) |
| file_search.c | 171 | ✅ | None | **REAL** — directory search |
| privacy_analysis.c | 170 | ✅ | None | Likely real |
| cloud_api.c | 176 | ✅ | None | Likely real |
| voice.c | 188 | ✅ | None | Likely real |
| local_model.c | 140 | ✅ | None | Likely real |
| hotword.c | 144 | ✅ | None | Likely real |
| proactive_protection.c | 164 | ✅ | None | Likely real |
| anonymity_coach.c | 124 | ✅ | None | Likely real |
| visual_feedback.c | 103 | ✅ | None | Likely real |
| kairos_window.c | 347 | ❌ needs xdg-shell header | None | **REAL** — built this session |

**Verdict:** ai.c + tools.c + memory.c are real, substantial code. The blocker is libcurl — ai.c and vision.c need it. The rest compile clean. guardian.c is stub-heavy (9 TODOs for monitoring that was never implemented).

### wlt-permissions/ (5 files, 1,390 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| manifest.c | 449 | ✅ | None | **REAL** — manifest parser |
| permissions.c | 343 | ✅ | None | **REAL** — permission grant/deny logic |
| persist.c | 323 | ✅ | None | **REAL** — file-backed persistence |
| audit.c | 166 | ✅ | None | **REAL** — audit log |
| badges.c | 109 | ✅ | None | **REAL** — privacy badge system |

**Verdict:** Cleanest orphaned code. All 5 files compile, no stubs, no TODOs. No main() — designed as a library. Ready to wire into wlt-sandbox/launch.c.

### wlt-tor-mgr/ (7 files, 2,246 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| tor_mgr.c | 708 | ✅ | None | **REAL** — has main(), CLI dispatch |
| control.c | 465 | ✅ | None | **REAL** — Tor control port protocol (GETINFO, SIGNAL) |
| circuits.c | 300 | ✅ | None | **REAL** — parses real circuit-status from Tor control port |
| config.c | 263 | ✅ | None | **REAL** |
| bridges.c | 235 | ✅ | None | **REAL** — bridge configuration |
| identity.c | 100 | ✅ | None | **REAL** — sends NEWNYM via control port |
| tor_vm.c | 175 | ❌ fcntl | 2 TODOs (seL4 VMM) | Expected — seL4 VMM not built yet |

**Verdict:** circuits.c reads REAL Tor control port data (`GETINFO circuit-status`), parses circuit lines, resolves node info. identity.c sends real `SIGNAL NEWNYM`. This is NOT placeholder — it's real Tor control protocol code. **BUILD IT.**

### wlt-pkg/ (6 files, 984 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| pkg.c | 256 | ✅ | 5 TODOs (install/remove/update flows) | **STUB** — skeleton with TODOs |
| download.c | 222 | ✅ | None | Likely real |
| verify.c | 167 | ✅ | None | Likely real |
| transaction.c | 140 | ✅ | None | Likely real |
| store.c | 125 | ✅ | None | Likely real |
| dependency.c | 74 | ✅ | 4 TODOs + `g_db_count_hack` returns 0 | **STUB** |

**Verdict:** Compiles clean but pkg.c (the core) has 5 TODOs for install/remove/update flows. dependency.c has a function literally named `g_db_count_hack` that returns 0. The actual package install logic was never written. **Audit only — not ready to wire.**

### wlt-update/ (4 files, 1,667 lines)

| File | Lines | Compiles | Stubs | Verdict |
|------|-------|----------|-------|---------|
| update.c | 711 | ✅ | None found | Likely real |
| partition.c | 441 | ✅ | None found | Likely real |
| config.c | 283 | ✅ | None found | Likely real |
| verify.c | 232 | ✅ | None found | Needs libsodium |

**Verdict:** Clean compile, no TODOs. Needs libsodium for Ed25519 verification. No main() — library only. Lower priority than A-C. **Audit only for now.**

## Key Finding for Section B

**circuits.c is REAL, not placeholder.** It sends `GETINFO circuit-status` to Tor's control port, parses the response into circuit structs with relay fingerprints, resolves node info via `GETINFO ns/id/<fp>`, and even does country lookup via `GETINFO ip-to-country/<ip>`. identity.c sends `SIGNAL NEWNYM`. This is production-grade Tor control protocol code.

## Key Finding for Section A

**ai.c is REAL** (921 lines) but needs libcurl. It builds OpenAI-compatible chat JSON, sends HTTP POST to llama-server at 127.0.0.1:8080, parses tool_calls from the response. tools.c (524 lines) has 15+ real tool implementations. memory.c (422 lines) has real conversation persistence. The blocker is libcurl in the rootfs — without it, ai.c won't compile.
