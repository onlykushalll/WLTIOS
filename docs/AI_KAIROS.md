# WLTIOS — AI: Kairos + Guardian (Consolidated Reference)

> **Supersedes:** `docs/specs/wlt-ai.md`, `docs/specs/kairos-instruction.md`,
> `docs/adr/08-kairos.md` (kept for historical decision rationale, not
> archived - see note below), `docs/context/WLT_AI_AUDIT.md`,
> `WLTIOS_GUARDIAN_HANDOFF.md`, and `src/userspace/wlt-defender/docs/*`.
> Archived under `_ARCHIVED_2026-07-29`. ADR-08 is superseded in substance
> by the Defender-style decision below but is left in `docs/adr/` unarchived
> since ADRs are meant to be a permanent decision record, not a status doc -
> a note has been added there is unnecessary since the ADR already reads as
> historical ("Decision made 2026-06-22").

---

## 1. Two separate things, often conflated in older docs

WLTIOS ships **two distinct AI-adjacent systems**, and a lot of earlier
documentation blurred them together:

1. **Kairos** - the OS's conversational/tool-using assistant, currently
   backed by a small local LLM (Qwen2.5-1.5B as a stand-in for a larger
   model the project intends to swap in later).
2. **Guardian** (`wlt-defender`) - the always-on endpoint-security engine.
   **Deliberately not LLM-based** - deterministic signature/behavioral/
   integrity/anomaly/network layers, with Kairos (or a larger on-demand
   model) available as an optional, human-invoked investigator for the
   rare cases the deterministic layers can't resolve on their own.

The original `docs/specs/wlt-ai.md` describes an aspirational unified
"WLT-AI" with VLM/STT/TTS/hotword/cloud routing in Rust+Python. The actual
implementation is C, and most of that surface area is either real-but-unwired
or explicitly out of scope for now (see §3).

---

## 2. Kairos - what's real today

Independently corroborated via the project's own orphaned-code audit
(2026-07-20) plus this consolidation's spot checks:

| Component | File | Status |
|---|---|---|
| Core dispatch, HTTP chat, tool-call parsing | `ai.c` (778 lines) | Real. Needs libcurl (or the curl-free rewrite path - see below). |
| Tool implementations (15+) | `tools.c` (451 lines) | Real. See `docs/SECURITY.md` §4 for the `popen` vs. `execv` split - `run_command` is still shell-based and denylist-gated; `change_setting` was rewritten to `fork`+`execv`. |
| Conversation memory | `memory.c` (352 lines) | Real, file-backed persistence. |
| Config parsing | `config.c` (441 lines) | Real. |
| MCP protocol bridge | `kairos_mcp.c` (405 lines) | Real. |
| Screen understanding (VLM) | `vision.c` (497 lines) | Real image-analysis code; needs libcurl. Not yet wired into the live Kairos window per the PRD's own "in progress" note. |
| Wayland client window (sidebar) | `kairos_window.c` (320 lines) | Real - calls `wlt_ai_query` in-process (no more `fork()`+`execl("/bin/sh"...)`, which was a real command-injection path, now closed per the PRD). Handles keyboard input. AI engine initializes with local LLM at boot per verified serial output ("`[kairos] AI engine ready (local LLM)`", commit `b5570c5`). |
| Privacy pipeline (PII strip + Tor routing for cloud calls) | `privacy_pipeline.c` (228 lines) | Real, no stubs. |
| Anonymity coach (Tor/DNS/MAC/kill-switch health) | `anonymity_coach.c` (109/124 lines across audits) | Real, no stubs. Independently confirmed wired into `kairos_window.c` via `wlt_ai_anon_check` (git commit `4214a32`, re-verified by reading `git show HEAD:...` directly per the PRD's own methodology). |
| Privacy/app-risk analysis | `privacy_analysis.c` (147/170 lines) | Real, no stubs. `wlt_ai_privacy_analyze` similarly confirmed wired. |
| File search | `file_search.c` (149 lines) | Real. |
| Proactive protection (port-scan/brute-force/suspicious-process alerts) | `proactive_protection.c` (143/164 lines) | Real logic; not yet wired to Guardian's socket per the audit. |
| Cloud API config (Gemini/Claude/GLM) | `cloud_api.c` (151/176 lines) | Real config management; no evidence any cloud call has actually been exercised in a verified session. |
| Voice I/O wrapper | `voice.c` (160/188 lines) | Real wrapper; depends on STT/TTS functions in `ai.c` that are themselves curl-dependent stubs per the audit. Needs real hardware mic to meaningfully test - audit-only status. |
| Hotword detection | `hotword.c` (122/144 lines) | Energy-based (RMS threshold) placeholder, not a real keyword-spotting model - explicitly documented as such in its own audit entry. |
| Local model state | `local_model.c` (122/140 lines) | Has a `void *ctx` opaque pointer for a future llama.cpp context; intentionally not connected, per the project's explicit "no AI models on my RAM" constraint from the project owner. |
| Sidebar data model | `sidebar.c` (158/183 lines) | Real data model (messages, visibility, status); its own `render()` function is a no-op stub. `kairos_window.c` is the actual rendering surface today - the audit's own conclusion was that `sidebar.c`'s render should be wired to `kairos_window.c`, not built as a second Wayland client. |
| Guardian bridge | `guardian.c` (in `wlt-ai/`, 286/342 lines) | Heavily stub (9-17 TODOs depending on audit pass, all marked `TODO(kairos):` for future Guardian-alert integration). The actual monitoring logic these TODOs would consume lives in `wlt-defender`, not here. |

**Model status:** Qwen2.5-1.5B is the proven local baseline (real
end-to-end pipeline against a mock LLM server, per the PRD: guest POST
reaches the mock server, 200 response, parsed and displayed in the Kairos
window - commit `ad0e2c9`). A larger model (referred to in various docs as
Qwythos-9B or similar) is intentionally deferred until a bootable Debian
live-USB frees enough real RAM to test it - this is a deliberate, explicit
project-owner decision, not a technical blocker.

**Kairos master system prompt:** `docs/specs/kairos-instruction_ARCHIVED_2026-07-29.md`
(content preserved, not lost) defines Kairos's identity, anti-hallucination
rules (every claim must cite evidence from a real command; confidence rated
0-100%, below 70% defers to human review), and refusal policy. This prompt
text is a design artifact, not something this consolidation re-verified
against a live model's actual behavior.

---

## 3. Guardian (wlt-defender) - what's real today

### 3.1 Why Defender-style, not LLM-first

`WLTIOS_GUARDIAN_HANDOFF_ARCHIVED_2026-07-29.md` (2026-06-26) documents the
reasoning: an always-on LLM is too slow (tokens/sec vs. millisecond
detection needs), too RAM-hungry (a 9B model is ~40x the entire OS's idle
RAM budget), and hallucination-risky for autonomous security actions.
Windows Defender's own architecture (signature DB + heuristics + small
classical ML, cloud LLM only as a separate paid analyst product, not in the
detection loop) is cited as the model being followed. This reasoning reads
as sound and is presented as such, not merely repeated uncritically.

### 3.2 The 5 layers + coordinators (source: `src/userspace/wlt-defender/src/`)

| Layer | File | Purpose |
|---|---|---|
| L1 Signatures | `sig_engine.c` | YARA pattern matching |
| L2 Behavioral | `ebpf_engine.c` + `bpf/guardian.bpf.c` | eBPF syscall hooks, inline `SIGKILL` enforcement |
| L3 File integrity | `fim_engine.c` | SHA-256 + sqlite3, replaces AIDE |
| L4 Anomaly ML | `anomaly_engine.c` + `iforest.c` | Isolation forest, native C (no Python subprocess in the current build, per the README's own note that it moved off the original Python-bridge design) |
| L5 Network IDS | `net_engine.c` | Tor-focused rules |
| + | `canary_engine.c`, `ransomware_engine.c`, `pmu_engine.c` | Canary-file ransomware detection, rate-based ransomware behavior, hardware perf-counter side-channel/miner detection |
| Enforcement | `action_engine.c` | Tiered: reversible actions (freeze, quarantine, block-net) autonomous; irreversible (kill, delete, kill-switch) need confirmation |
| Policy | `policy_engine.c` | Deterministic threat-to-action mapping, not LLM-decided |
| Forensic log | `forensic_log.c` | SHA-256 hash-chained + AES-256-GCM encrypted, tamper-evident |

**Development-environment caveat that matters:** the bulk of this code was
first built and self-tested in a separate cloud sandbox
(`src/userspace/wlt-defender/docs/DEV_ENV_ARCHIVED_2026-07-29.md` documents
this explicitly: no root, no `CAP_BPF`, eBPF load/attach untested there,
enforcement actions simulated). The code was subsequently brought into this
repo's real tree (the `.c` files are present at
`src/userspace/wlt-defender/src/`, confirmed by directory listing) and
integrated into the real boot sequence as PID 3 (per `git log` entries like
`58ca83a`, "Guardian integrated: watchdog PID2 + guardian PID3 + shell
commands", 2026-06-27, and `64c7e30`, "install BPF+sigs+ML model in rootfs",
2026-07-24). This is a different situation from the "Substitution" failure
pattern documented elsewhere in this project's history (where sandbox work
never reached the real tree at all) - here it did land in the real repo -
but the sandbox-vs-target gap documented in `DEV_ENV.md` (eBPF load/attach,
real enforcement) should still be treated as "needs re-confirmation on a
real boot," not as proven merely because the source exists.

### 3.3 The 14-26 BPF syscall hooks

`bpf/guardian.bpf.c` implements inline-kill hooks (`bpf_send_signal(SIGKILL)`
fires before the syscall completes) for: `execve`, `execveat`, `openat`,
`openat2`, `connect`, `ptrace`, `clone`, `clone3`, `unshare`, `setns`,
`bind`, `socket`, `setuid`, `capset`, plus later additions covering
`sched_process_exit`, `ioctl`/`rtnl_newroute` (kill-switch-reversal
blocking), `write` (byte-rate anomaly feature), `unlink`/`rename` families
(ransomware bulk-op counters), and `sched_process_fork` (ancestry tracking)
- different docs cite different totals (14 in the MANIFEST, 23 in
MASTER_CONTEXT, 26 in the README) depending on which addition-phase they
were written after; **the README's "The 14+ BPF syscall hooks" table is the
most complete enumeration found** and is the one to trust for the current
count.

**Attachment status:** see `docs/SECURITY.md` §5 - real attach code exists,
tracefs-mount fix exists, but a fresh confirmation that all hooks actually
attach on a real root/CAP_BPF boot was not re-run in this pass.

### 3.4 Guardian's own audits (kept as genuine findings, not just claims)

- **Threat model** (`THREAT_MODEL_ARCHIVED_2026-07-29.md`): walks through
  4 concrete bypass attempts layer-by-layer and honestly identifies one
  unresolved gap - **slow (low-and-slow) data exfiltration via Tor**, below
  all anomaly/rate thresholds - as accepted residual risk requiring future
  volume-based anomaly detection.
- **Memory safety** (`MEMORY_SAFETY_AUDIT_ARCHIVED_2026-07-29.md`): no
  critical issues found across 9 source files; the one open item was the
  MCP server's JSON-RPC parser (largest external-input attack surface),
  flagged as needing a dedicated fuzz pass rather than closed out.
  `tests/fuzz_mcp.py` exists in the tree for this purpose.
- **Race conditions** (`RACE_AUDIT_ARCHIVED_2026-07-29.md`): identifies a
  real potential ABBA deadlock between the event-bus/log/FIM locks and
  explains specifically why the "Bug 8 fix" (copy the subscriber list,
  release the bus lock, *then* invoke callbacks) breaks the cycle. This is
  a substantive finding, not a rubber stamp.
- **TPM key sealing** (`TPM_KEY_SEALING_ARCHIVED_2026-07-29.md`): honestly
  documents that the forensic log's encryption key currently comes from
  `/dev/urandom`, not TPM-sealed PCR values, because QEMU TPM emulation
  isn't configured in this build - the TPM path is a documented sketch, not
  implemented.

### 3.5 On-demand consultant / MCP tools

`kairos_cli.c`, `mcp_server.c` (per the MANIFEST, a 32-tool JSON-RPC-over-
Unix-socket server distinct from `wlt-ai`'s own `kairos_mcp.c`) exist in
`wlt-defender/`. The consultant model choice (a larger, security-tuned model
for on-demand investigation, with Qwen2.5-1.5B as the fallback on
lower-RAM targets) mirrors the Kairos model discussion in §2 - not yet
exercised against a real large model in any verified session for the same
"no AI models on my RAM" reason.

---

## 4. What to trust vs. re-verify

**Solid (independently spot-checked or corroborated across multiple
independent project audits in this pass):** the `popen`/`execv` split in
`tools.c`; the fail-closed permission-manager behavior; the LUKS wiring;
`anonymity_coach`/`privacy_analysis` being real and wired; the memory-safety
and race-condition findings for Guardian.

**Needs a fresh boot to confirm before relying on it:** actual eBPF hook
attachment count and behavior on a real (non-sandbox) boot; whether the
AI's tool-execution path has since been routed through `wlt-permissions`
(an active track as of the last PRD update, status not re-confirmed here).
