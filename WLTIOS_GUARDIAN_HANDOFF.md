# WLTIOS GUARDIAN — Master Build Specification & Handoff Context

**Document version:** 1.0
**Date:** 2026-06-26
**Purpose:** Complete context for handing off the WLTIOS Guardian (security engine) build to external AI builders/hardeners.

**Handoff plan:**
- **GLM 5.2** → Build the Guardian engine per this spec
- **Flash 3.5** → Security & vulnerability hardening pass on the built code
- **Opus / Sonnet 4.6** → Bug detection, edge-case fuzzing, final hardening

---

## 0. Read This First — What WLTIOS Is

WLTIOS (We Love Tor — Intelligent OS) is a hybrid seL4 + Linux privacy-focused operating system that routes ALL traffic through Tor. It is NOT a normal Linux distro. Key facts:

- **Microkernel:** seL4 (formally verified, capability-based)
- **Guest kernel:** Debian 5.10.0-30 (i386 for 32-bit, amd64 for 64-bit) running as a seL4 VM
- **Userspace:** Custom-built minimal (no systemd, no glibc unless explicitly added, static binaries where possible)
- **Networking:** ALL traffic through Tor. Kill switch blocks non-Tor egress.
- **Amnesic mode:** /var, /tmp, /run, /home are tmpfs — wiped on reboot (Live USB style)
- **No sudo in build environment** (WSL2 Ubuntu, user `kushal`, no root)
- **No kernel modules can be loaded at runtime in 64-bit guest** (Debian kernel has CONFIG_IP_NF_IPTABLES=m but the .ko is not bundled; kill switch uses route-based SIOCDELRT fallback as a result)

**Current verified state (QEMU-tested, not just "compiles"):**
- 32-bit guest: ~150 MB total RAM, 10/10 integration tests pass, Tor 100%, iptables kill switch (built-in module), compositor, shell, sandbox, MAC randomization, amnesic mode
- 64-bit guest: ~512 MB QEMU allocation, Tor 0.4.9.6 (glibc) bootstraps 100%, route-based kill switch (SIOCDELRT, no ip_tables.ko needed), e1000 network, wlroots 0.18 headless compositor, wayland-0 socket, 15-command shell

**Source code location:** `C:\Users\Default.L-HCG-9FVVGS3\OneDrive\Desktop\WLTIOS\` (Windows path, accessible from WSL2 as `/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/`)
- `src/userspace/` — all userspace C source (wlt-init, wlt-shell, wlt-net, wlt-ai, wlt-compositor, wlt-crypto, wlt-sandbox, wlt-permissions, wlt-pkg, wlt-render, wlt-settings, wlt-terminal, wlt-filemanager, wlt-tor-browser, wlt-tor-mgr, wlt-update, wlt-widgets)
- `scripts/build-initramfs.sh` — 32-bit build
- `scripts/build-initramfs64.sh` — 64-bit build
- `docs/adr/` — architecture decision records
- `docs/specs/` — specifications
- `tests/integration/run-all.sh` — 10 integration tests

---

## 1. The Decision: Defender-Style Core, Not LLM-First

### Why we rejected the LLM-first approach
The original Kairos design (ADR-08) had an always-on LLM (Llama 3 8B / Phi-3) doing security reasoning. We rejected this because:

1. **RAM cost:** A 9B model (Qwythos) uses ~6 GB active. The entire WLTIOS 32-bit guest uses 150 MB. The AI would use 40× more RAM than the OS.
2. **Speed:** LLMs generate ~50 tokens/sec. Defender-class detection needs millisecond response. An LLM cannot keep up with raw kernel events.
3. **Hallucination risk:** A security AI that hallucinates is catastrophic — it either triggers the kill switch on phantom threats (locking the user out) or lets real malware run.
4. **Windows Defender (the most widely-deployed security product on Earth) does NOT use an LLM for detection.** Its engine is: signature DB + heuristic rules + small classical ML classifiers (gradient-boosted trees, not neural nets). The LLM "Security Copilot" is a separate paid product for analysts, not part of the detection loop.

### The chosen architecture: Defender-style core + optional AI consultant

```
ALWAYS-ON CORE (~80-150 MB, defender-style, deterministic + small ML)
  Falco (eBPF behavioral rules)        ──┐
  YARA (signature matching)            ──┤
  AIDE (file integrity monitoring)     ──┼── 95%+ of threats caught here
  Anomaly ML (isolation forest)        ──┤    instant, deterministic, cheap
  Network IDS (Suricata-lite)          ──┘    Tor-only traffic, lighter rules
        │
        │ flagged event (rare, novel, ambiguous — <5% of cases)
        ▼
ON-DEMAND CONSULTANT (optional, NOT automatic, user-launched)
  Qwythos-9B-Claude-Mythos-5-1M (Q4_K_M, 5.24 GB)
  - Spins up ONLY when user explicitly invokes investigation
  - Ingests full context (1M token window)
  - Reasons about novel/complex attacks
  - Suggests action → user confirms → core engine executes
  - Spins DOWN after → back to 0 GB
```

**RAM budget:**
- Idle: ~80-150 MB (core engine) + 20 MB (Kairos daemon) = **~100-170 MB total**
- During investigation: + 6 GB (Qwythos) = ~6.1 GB
- After investigation: back to ~100-170 MB

**The core engine must be good enough that the AI is almost never needed.** This is the design goal.

---

## 2. The Core Engine — WLTIOS Guardian

### 2.1 Detection layers (in priority order)

#### Layer 1: Signature matching (instant, ~0 CPU)
- **Tool:** YARA (pattern matching) + a frozen signature DB bundled on the bootable USB
- **What it catches:** Known malware hashes, known-bad file patterns, known exploit strings
- **Signature source:** Curated YARA rules (community + custom WLTIOS-specific rules for seL4/Linux-guest attacks)
- **Update strategy:** Hybrid — frozen baseline on USB (works offline, day-zero) + fetch fresh rules over Tor on boot when network available. Store updates in persistent /etc/wlt/guardian/sigs/ (NOT tmpfs — persists across reboots on USB, but amnesic mode wipes RAM-only paths)
- **RAM:** ~5-20 MB depending on DB size

#### Layer 2: Behavioral rules via eBPF (real-time, ~1-5% CPU)
- **Tool:** Falco (CNCF graduated, eBPF-based, lowest RAM of all eBPF security tools per benchmarks)
- **Why eBPF:** No kernel modules needed (critical — we cannot load .ko files in 64-bit guest). eBPF programs load via the BPF syscall and JIT-compile in-kernel.
- **What it catches:** Behavioral anomalies — "process X opened /etc/shadow then connected to external IP", "shell spawned by web server", "unexpected file in /etc", etc.
- **Rule language:** Falco's DSL — write custom rules for WLTIOS-specific threats (Tor disconnect, kill switch bypass attempts, sandbox escapes, cap-space violations)
- **Enforcement:** Falco alerts natively. We add a thin enforcement layer that reads Falco alerts and takes action (kill, freeze, quarantine, kill-switch). Consider Tetragon's enforcement approach for inspiration.
- **RAM:** ~30-50 MB (Falco daemon + rule engine)
- **Critical constraint:** Falco's userspace daemon may need libs not in our minimal initramfs. **Build risk: medium.** Mitigation: try building Falco statically; if it fails, write a minimal custom eBPF loader using libbpf (the kernel's BPF syscall is available).

#### Layer 3: File integrity monitoring (periodic, ~5 MB)
- **Tool:** AIDE (Advanced Intrusion Detection Environment)
- **What it catches:** Unauthorized modification of critical files (/etc, /bin, /sbin, /usr/bin, /lib, seL4 config, torrc, etc.)
- **Baseline:** Generated at build time, stored on read-only USB. Compared against live filesystem periodically (every 5 min) and on-demand.
- **Action:** Any mismatch → immediate alert + quarantine the modified file (move to /var/quarantine, replace with baseline copy)
- **RAM:** ~5 MB

#### Layer 4: Anomaly detection (small classical ML, ~10-20 MB)
- **Model:** Isolation forest or autoencoder trained on system metrics (CPU, RAM, disk I/O, network flow stats, syscall rates per process)
- **NOT an LLM, NOT a neural net** — sklearn-classical ML, kilobytes to megabytes
- **What it catches:** Novel attacks signatures miss (zero-days) — unusual process behavior patterns, resource consumption anomalies, timing-based attacks
- **Training:** Pre-trained on benign workloads at build time, bundled as a pickled model (~50 KB-2 MB). Online learning optional (amnesic mode complicates this).
- **RAM:** ~10-20 MB (model + inference)
- **False-positive control:** Anomaly score threshold tuned high (only flag >3σ deviations). Anomaly alerts go to Layer 5 review, not direct action.

#### Layer 5: Network IDS (optional, moderate)
- **Tool:** Suricata-lite or custom af_packet sniffer with a small ruleset
- **Why lighter than full Suricata:** WLTIOS traffic is Tor-only (already encrypted, already anonymized). The main network threat is pre-Tor leakage (which the kill switch handles) and Tor circuit compromise. Rules focus on: DNS leaks, non-Tor connections, Tor protocol anomalies, known-malicious .onion addresses.
- **RAM:** ~20-40 MB if enabled; can be disabled on low-RAM targets
- **Action:** Any non-Tor egress → instant kill switch (already implemented, route-based fallback works)

### 2.2 Enforcement actions (what the engine can DO)

Tiered by reversibility — autonomous actions are reversible, irreversible actions need user confirmation (unless threat is critical and user is unreachable, in which case the engine takes the safest irreversible action and logs heavily).

| Action | Reversibility | Autonomous? | Implementation |
|---|---|---|---|
| **Log + alert** | N/A | ✅ Always | syslog + notification daemon |
| **Freeze process** | ✅ Reversible | ✅ Autonomous | cgroup freezer (if available) or SIGSTOP |
| **Quarantine file** | ✅ Reversible | ✅ Autonomous | move to /var/quarantine, record original path |
| **Quarantine process** | ✅ Reversible | ✅ Autonomous | move PID to isolated sandbox namespace (unshare CLONE_NEWNET) |
| **Block network flow** | ✅ Reversible | ✅ Autonomous | iptables (if available) or route manipulation |
| **Kill process** | ❌ Irreversible | ⚠️ Critical-only | SIGKILL (with full forensic snapshot first) |
| **Activate kill switch** | ❌ Irreversible | ⚠️ Critical-only | route deletion (SIOCDELRT) — blocks ALL non-Tor traffic |
| **Delete file** | ❌ Irreversible | ❌ Never autonomous | secure delete (3-pass overwrite + unlink) — user confirm always |
| **Encrypt file** | ❌ Irreversible | ❌ Never autonomous | Key Vault — user confirm always |

**Critical-only autonomous actions** trigger when:
- Threat is classified as critical (signature match for known-critical malware, or behavioral rule with critical severity)
- AND delaying for user confirmation would cause more harm than acting
- Engine takes the action, takes a forensic snapshot, logs everything, and notifies user post-hoc

### 2.3 The Guardian daemon (`wlt-guardian`)

A single unified daemon that orchestrates all 5 layers. Lives in `src/userspace/wlt-guardian/`. Architecture:

```
wlt-guardian (PID 2, started by wlt-init after network)
├── sig_engine    — YARA signature matching (file scan, process memory scan)
├── ebpf_engine   — Falco rules + eBPF probes (syscall, net, file events)
├── fim_engine    — AIDE file integrity monitoring
├── anomaly_engine — Isolation forest inference on metrics
├── net_engine    — Network IDS (optional)
├── action_engine — Executes enforcement actions (freeze/kill/quarantine/etc.)
├── policy_engine — Decides which action for which threat (rules-based, NOT LLM)
├── forensic_log  — Encrypted, append-only forensic log (tamper-proof)
└── api_socket    — Unix socket for wlt-shell / wlt-settings / Kairos to query status
```

**Output to user:** NOT a chatbot. The guardian:
- Shows notifications via the compositor (status bar updates)
- Logs to /var/log/guardian.log (encrypted, append-only)
- Exposes a `guardian` shell command: `guardian status`, `guardian alerts`, `guardian history`, `guardian quarantine list`
- Does NOT explain in natural language unless the user explicitly invokes the optional Kairos consultant

### 2.4 Threat model (what we're defending against)

**In scope:**
- Known malware (signature-caught)
- Behavioral anomalies (process doing suspicious things)
- File tampering (integrity violations)
- Network attacks (port scans, non-Tor leakage, DNS leaks)
- Data exfiltration (unusual outbound transfers)
- Process escape attempts (sandbox breakouts, cap-space violations)
- Resource exhaustion attacks (fork bombs, disk fill)
- Persistence attempts (writing to /etc, cron, boot sectors)
- Tor circuit compromise (guard rotation anomalies, consensus attacks)
- seL4 capability violations (unauthorized cap creation/derivation) — phase 2, custom

**Out of scope (for v1):**
- Physical attacks (evil maid) — mitigated by amnesic mode + USB read-only, not the guardian
- Side-channel attacks (timing, power) — hardware problem
- Zero-day kernel exploits in seL4 itself — seL4 is formally verified, but if seL4 falls we fall
- Supply chain attacks on the build — mitigated by reproducible builds (future), not the guardian

---

## 3. The Deep-Kernel MCP (for Qwythos to use when invoked)

When the user manually launches a Kairos investigation, Qwythos-9B needs tools to inspect the system. This is a **separate MCP layer from the Guardian** — the Guardian is always-on and autonomous; the MCP is invoked only during human-driven investigations.

### 3.1 Design: reuse MCPilot's blueprint, build WLTIOS-native tools

MCPilot (the existing bridge on the user's Windows PC) has 33 tools (file ops, process, system, git, archive). We reuse its **architecture** (tool registry, JSON-RPC, safety denylist, user-confirm gates) but the tools themselves are WLTIOS-native.

### 3.2 Tool categories (~40-50 tools total)

#### Userspace tools (port from existing kairos_mcp.c — 12 tools, already written)
- `process_inspect(pid)` — memory map, open files, sockets, capabilities
- `process_kill(pid)` — SIGKILL (user confirm)
- `process_freeze(pid)` — SIGSTOP / cgroup freezer
- `process_quarantine(pid)` — move to isolated namespace
- `file_forensics(path)` — hash, creation chain, who touched it, last access
- `file_encrypt(path)` — Key Vault (user confirm)
- `file_quarantine(path)` — move to /var/quarantine
- `secure_delete(path)` — 3-pass shred + unlink (user confirm)
- `shell_exec(cmd)` — sandboxed + denylist + user confirm
- `browser_navigate(url)` — Tor-routed, phishing-checked
- `browser_scrape(url)` — extract text, no JS
- `install_package(path)` — wlt-pkg (user confirm)

#### Linux kernel tools (eBPF-based, no modules needed)
- `ebpf_load_probe(prog)` — load an eBPF tracing program
- `syscall_trace(pid, duration)` — trace a process's syscalls for N seconds
- `netflow_capture(duration)` — capture packet flows for N seconds
- `audit_query(filter)` — query the audit subsystem
- `kernel_mem_read(offset, len)` — read kernel memory (via /dev/mem or seL4 mapping)
- `perf_counters(cpu)` — CPU performance counters (cache misses, branch mispredicts — malware signatures)
- `intel_pt_trace(pid, duration)` — Intel Processor Trace (every branch executed — full replay)

#### seL4 layer tools (phase 2, custom)
- `sel4_caps_dump()` — dump entire capability space
- `sel4_ipc_trace(duration)` — trace IPC messages between components
- `sel4_sched_observe()` — observe TCB states, timeslices
- `sel4_fault_inject(target, fault)` — inject faults for red-team testing

#### Hardware tools
- `tpm_attest()` — TPM attestation (am I running what I think I am?)
- `ima_measure()` — Integrity Measurement Architecture (file integrity chain)

#### Guardian integration tools (read-only, for investigation)
- `guardian_status()` — current threat level, active alerts
- `guardian_history(since)` — historical alerts
- `guardian_forensics(event_id)` — full forensic snapshot for an event
- `guardian_sigs_list()` — current signature DB version + count

### 3.3 Safety: denylist + confirm gates (from MCPilot)

Dangerous commands blocked: `rm -rf /`, `mkfs`, `dd if=`, `shutdown`, `reboot`, `halt`, fork bombs, `chmod -R 777 /`, `chown -R`, `kill -9 1`, `kill -9 -1`, `wget http | sh`, `curl http | bash`.

Irreversible actions (kill, delete, encrypt, kill-switch) require user confirmation via the compositor UI.

### 3.4 Hallucination guardrails (architectural, not prompt-only)

When Qwythos is invoked:
1. **Grounding rule:** Every claim MUST cite evidence (PID, log line, packet hash). No citation → no action.
2. **Verification loop:** Before any action, re-verify via a tool. "PID 1234 is malicious" → `process_inspect(1234)` → confirm before `process_quarantine(1234)`.
3. **Confidence threshold:** Model must self-rate confidence. Below threshold → log + alert human, don't act.
4. **Reversibility tiers:** See section 2.2. Reversible = autonomous; irreversible = always human-confirm.
5. **Self-correction:** Qwythos has this (7/8 cited answers with tools per Empero's eval). Build the agent loop to require it: act → observe → confirm → if not, self-correct.

---

## 4. Model Choices (confirmed)

### Always-on: NO LLM (defender-style core only)
The Guardian is deterministic code + small classical ML. No LLM in the always-on loop.

### On-demand consultant: Qwythos-9B-Claude-Mythos-5-1M
- **File:** `empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF` (Q4_K_M, 5.24 GB)
- **Why:** Native Qwen3.5-style function calling (no wrapper needed), 1M token context (ingest full forensic logs), self-corrects with tools (7/8 cited correct answers), explicitly trained on cybersecurity, deeply uncensored (won't refuse legitimate security ops like "dump this process's memory"), built for agentic workflows
- **Loaded only when user invokes `kairos investigate <event>` or `kairos explain <question>`**
- **Runs via llama.cpp's llama-server on port 8080** (already built and verified in WSL2)

### Fallback consultant (low-RAM targets, <8GB): Qwen2.5-1.5B-Instruct
- **Why:** Native function calling (BFCL-tested, conservative — declines what it can't handle), small (1.4 GB Q4), fast
- **When:** Hardware check at boot. If RAM < 8GB, use Qwen2.5-1.5B as consultant. If < 4GB, no consultant (Guardian-only mode).

### DROPPED: VibeThinker-3B
- **Why:** Explicitly not trained for tool-calling or agents (per HuggingFace model card). Brilliant at verifiable reasoning (math, code competitions) but wrong tool class for security agent work.

---

## 5. Build Environment & Constraints

### 5.1 Build environment
- **Host:** Windows 11 PC (KUSHALPC), accessed via WSL2 Ubuntu
- **WSL2 user:** `kushal` (NO sudo — cannot apt install packages system-wide)
- **Bridge:** Next.js /api/wsl route → MCPilot (port 7878) → WSL2. All bash commands run via temp script files to avoid cmd.exe escaping issues.
- **Compiler:** gcc 15.2.0 (Ubuntu), available in WSL2
- **QEMU:** qemu-system-x86_64 and qemu-system-i386, available in WSL2
- **Git:** Repo at `/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS/`

### 5.2 Hard constraints (NON-NEGOTIABLE)
1. **No sudo / no root** — cannot install system packages. Must build statically or bundle libs.
2. **No kernel module loading in 64-bit guest** — Debian kernel has things as modules but .ko files aren't bundled. Use eBPF (BPF syscall) instead of .ko where possible.
3. **Static linking preferred** — dynamically-linked binaries need their libs bundled in initramfs. Static = simpler.
4. **RAM-constrained** — 32-bit target ~150 MB total, 64-bit target ~512 MB. The Guardian must fit in ~80-150 MB.
5. **Amnesic** — /var, /tmp, /run, /home are tmpfs. Persistent state goes on the USB (mounted read-only by default, read-write for specific paths like /etc/wlt/guardian/sigs/ for signature updates).
6. **Tor-only networking** — all non-Tor egress blocked by kill switch. Signature updates must route through Tor (or be bundled offline).
7. **No "compiled therefore done"** — every feature must be verified in QEMU serial output before claiming done.

### 5.3 Build commands (existing)
```bash
# 32-bit (works, all features)
bash scripts/build-initramfs.sh

# 64-bit (works, all features post-Phase-5 fixes)
bash scripts/build-initramfs64.sh

# Test in QEMU (64-bit example)
qemu-system-x86_64 -kernel kernel-amd64 \
  -initrd wltios-initramfs64.cpio.gz \
  -append "console=ttyS0 root=/dev/ram rdinit=/init" \
  -m 512 -nographic -no-reboot \
  -netdev user,id=n0 -device e1000,netdev=n0
```

### 5.4 Existing code to reuse / port
- `src/userspace/wlt-ai/kairos_mcp.c` — 12 userspace MCP tools (already written, with safety denylist). Port to the new WLTIOS-MCP.
- `src/userspace/wlt-shell/cmds/shell.c` — has `killswitch` command (route-based fallback, SIOCDELRT). Guardian's network-blocking action can reuse this.
- `src/userspace/wlt-shell/cmds/shell.c` — has `sandbox` command (unshare CLONE_NEWNET). Guardian's process-quarantine can reuse this.
- `src/userspace/wlt-init/init-minimal.c` — PID 1, mounts tmpfs, loads e1000.ko, starts services. Guardian should be started here as PID 2 (after wlt-net, before/alongside Tor).
- `mcpilot/mcpilot.cjs` — MCPilot's architecture (tool registry, JSON-RPC, 33-tool structure). Reuse the blueprint for WLTIOS-MCP.

### 5.5 Git state
- Latest commits on `main` branch:
  - `6a825e0` — [docs] Phase 5 final status report
  - `fe99c1c` — [fix] 64-bit kill switch WORKS: route-based fallback
  - `4a52fef` — [fix] 64-bit Tor WORKS: glibc Tor 0.4.9.6

---

## 6. What to Build (the actual task)

### For GLM 5.2 (builder):

**Build the WLTIOS Guardian — a Defender-class security engine that runs always-on in the WLTIOS guest.**

**Deliverables:**

1. **`src/userspace/wlt-guardian/`** — new module with:
   - `guardian.c` — main daemon (orchestrates all engines)
   - `sig_engine.c` — YARA signature matching
   - `ebpf_engine.c` — eBPF probe loading + Falco-style rule engine (or integrate Falco if it builds statically)
   - `fim_engine.c` — AIDE-style file integrity monitoring
   - `anomaly_engine.c` — isolation forest inference (bundle a pre-trained model)
   - `net_engine.c` — network IDS (af_packet sniffer with Tor-focused rules)
   - `action_engine.c` — enforcement actions (freeze/kill/quarantine/kill-switch)
   - `policy_engine.c` — threat→action mapping (rules-based, deterministic)
   - `forensic_log.c` — encrypted append-only log
   - `include/guardian.h` — public API

2. **`src/userspace/wlt-mcp/`** — new module with:
   - `mcp_server.c` — JSON-RPC server over Unix socket, tool registry
   - Port the 12 tools from `wlt-ai/kairos_mcp.c`
   - Add the ~30 new deep-kernel tools (eBPF, audit, process, network, seL4 phase-2)
   - Safety denylist + user-confirm gates (port from MCPilot)

3. **`src/userspace/wlt-kairos/`** — refactor existing `wlt-ai` into:
   - `kairos_cli.c` — shell command `kairos investigate|explain|status`
   - `kairos_loader.c` — loads/unloads Qwythos-9B (or Qwen2.5-1.5B fallback) on demand
   - `kairos_agent.c` — agent loop (act → verify → confirm → self-correct)
   - `instruction.md` — the master system prompt (see section 7)

4. **Update `src/userspace/wlt-init/init-minimal.c`** — start wlt-guardian as PID 2 after wlt-net, before Tor.

5. **Update `scripts/build-initramfs64.sh`** — build and bundle:
   - wlt-guardian (static)
   - wlt-mcp (static)
   - wlt-kairos (static)
   - YARA + signature DB (frozen baseline)
   - AIDE + baseline DB
   - eBPF programs (compiled, loaded at runtime)
   - Anomaly ML model (pickled, ~1 MB)
   - Qwythos-9B Q4_K_M (5.24 GB — on USB, NOT in initramfs; loaded on-demand)
   - Qwen2.5-1.5B fallback (on USB)

6. **Update `tests/integration/run-all.sh`** — add tests for:
   - Guardian detects known-malware signature → quarantines
   - Guardian detects behavioral anomaly → freezes process
   - Guardian detects file tampering → restores from baseline
   - Guardian detects non-Tor egress → kill switch activates
   - MCP tool execution (each tool returns expected result)
   - Kairos consultant loads on-demand, answers question, unloads

7. **`docs/adr/09-guardian.md`** — ADR documenting the Defender-style decision (supersedes parts of ADR-08)

**Verification requirement:** Every feature must be tested in QEMU with serial output showing the detection/action. "It compiles" is not done. "QEMU serial shows 'Guardian: threat detected, process 1234 quarantined'" is done.

### For Flash 3.5 (security hardening):

After GLM 5.2 builds it, harden the built code:
- Threat model review (does the engine defend what it claims?)
- Vulnerability audit (can an attacker disable/bypass the Guardian? race conditions? TOCTOU?)
- eBPF program safety (verifier-bypass risks, side-channel leaks)
- Forensic log tamper-proofing (can an attacker forge/delete entries?)
- Kill switch bypass (can an attacker restore the route before Guardian reacts?)
- Sandbox escape (can a quarantined process break out of unshare CLONE_NEWNET?)
- Signature DB poisoning (can an attacker ship malicious "updates"?)
- MCP tool abuse (can a Kairos hallucination cause irreversible damage despite guardrails?)

### For Opus / Sonnet 4.6 (bug detection + hardening):

After Flash 3.5 hardens it:
- Edge-case fuzzing (empty inputs, huge inputs, malformed JSON, concurrent tool calls)
- Memory safety audit (buffer overflows, use-after-free, integer overflow in C code)
- Race condition analysis (Guardian engine vs attacker process — who wins the race?)
- Resource exhaustion (can an attacker trigger 1000 alerts/sec and overwhelm the engine?)
- Test coverage gaps (what scenarios aren't tested?)
- Code quality (error handling, logging, graceful degradation when eBPF unavailable)

---

## 7. Kairos Master System Prompt (`instruction.md`)

This is the system prompt fed to Qwythos-9B on EVERY call when the user invokes the consultant. It must be precise, anti-hallucination, and context-rich.

**Requirements for the prompt (GLM 5.2 must write the actual prompt text):**
- Identity: Kairos, the on-demand security consultant for WLTIOS
- Context: explain WLTIOS architecture (seL4 + Linux guest, Tor-only, amnesic, what's normal)
- Scope: you are invoked for investigation/reasoning, NOT always-on. The Guardian engine handles automatic defense.
- Anti-hallucination rules:
  - Every claim MUST cite evidence (PID, log line, packet hash). No citation → no claim.
  - If you don't know, say "I don't have enough evidence to determine X." Never fabricate.
  - Before any action, re-verify via an MCP tool. Never act on assumption.
  - Rate your confidence (0-100%). Below 70% → recommend human review, don't act.
- Tool use: you have ~50 MCP tools. Use them to verify, don't reason in a vacuum.
- Action policy: you SUGGEST actions, the user CONFIRMS. For irreversible actions, always require explicit user confirmation.
- Output format: structured (findings, evidence, confidence, recommended action). Not chatty.
- Refusal policy: you are uncensored but responsible. You will perform legitimate security operations (memory inspection, process killing, network blocking) — these are your job. You will NOT perform operations that harm the user's data without confirmation.

---

## 8. Success Criteria

The build is done when ALL of these are true (verified in QEMU, not just compiled):

1. **Guardian boots** — `wlt-init` starts `wlt-guardian` as PID 2, serial shows "Guardian: all engines online (sig/ebpf/fim/anomaly/net)"
2. **RAM under 200 MB** — Guardian + Kairos daemon + Tor + compositor + shell total < 200 MB idle (32-bit) or < 350 MB (64-bit)
3. **Signature detection** — Drop a test malware-signature file → Guardian quarantines it within 1 second, serial shows the detection
4. **Behavioral detection** — Run a test "malicious" process (opens /etc/shadow + connects external) → Guardian freezes it within 2 seconds
5. **File integrity** — Modify /etc/passwd → Guardian detects within 5 minutes, restores from baseline, alerts
6. **Kill switch integration** — Guardian detects non-Tor egress → activates route-based kill switch (SIOCDELRT)
7. **MCP tools** — Each of the ~50 tools responds correctly to a test invocation
8. **Kairos on-demand** — `kairos investigate <event>` loads Qwythos-9B, answers with cited evidence, unloads. RAM returns to baseline after.
9. **Amnesic preservation** — Reboot wipes /var (including quarantine), but USB-bundled signatures/baselines persist
10. **Integration tests** — `tests/integration/run-all.sh` passes all tests (existing 10 + new Guardian tests)

---

## 9. Open Questions (resolved during this design session)

1. **Defender-style core?** YES.
2. **Keep Qwythos as optional consultant?** YES, but make Guardian so good it's rarely needed.
3. **Falco vs Tetragon vs custom?** Falco as base (lowest RAM), add Tetragon-style enforcement layer.
4. **Signatures: bundle or fetch?** HYBRID — frozen baseline on USB + fetch fresh over Tor on boot.
5. **Deep kernel?** YES — eBPF for syscall/net/file (phase 1), seL4 cap-space (phase 2).
6. **VibeThinker?** DROPPED — not trained for tool calling.
7. **Always-on LLM?** DROPPED — Guardian is deterministic code + small classical ML only.
8. **Does Defender chat?** NO. Guardian acts, logs, notifies. No conversation unless user explicitly invokes Kairos.

---

## 10. Reference Material

- **WLTIOS ADR-08** (Kairos original design): `docs/adr/08-kairos.md` — being superseded by ADR-09
- **MCPilot source:** `/home/z/my-project/mcpilot/mcpilot.cjs` (33-tool blueprint)
- **Existing Kairos MCP tools:** `src/userspace/wlt-ai/kairos_mcp.c` (12 tools, port to wlt-mcp)
- **Kill switch (route-based):** `src/userspace/wlt-shell/cmds/shell.c` `cmd_killswitch` (SIOCDELRT/SIOCADDRT)
- **Sandbox (namespace isolation):** `src/userspace/wlt-shell/cmds/shell.c` `cmd_sandbox` (unshare CLONE_NEWNET)
- **Phase 5 final status:** `PHASE5_FINAL_STATUS.md` on desktop
- **Qwythos model card:** https://huggingface.co/empero-ai/Qwythos-9B-Claude-Mythos-5-1M-GGUF
- **Qwen2.5 model:** https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct
- **Falco:** https://falco.org/ (CNCF graduated, eBPF runtime security)
- **YARA:** https://virustotal.github.io/yara/
- **AIDE:** https://aide.github.io/

---

**End of specification. Hand off this entire document to GLM 5.2 for building, then to Flash 3.5 for hardening, then to Opus/Sonnet 4.6 for bug detection + final hardening.**
