# WLTIOS Guardian

A Defender-style endpoint protection engine for WLTIOS (seL4 + Linux guest).

**Philosophy:** deterministic code + tiny classical ML catches 95% of threats
in milliseconds at ~100–170 MB RAM. No always-on LLM. An optional on-demand
consultant (Qwythos-9B) can be invoked manually for the 1-in-1000 novel case.

## Architecture (5 layers + enforcement)

```
TIER 0 — RAW EVENT STREAM (code, no AI)        ~0 CPU
  eBPF probes (bpf/) + FIM inotify + audit
  → signature match (instant) | threshold (flag) | anomaly> X (flag)
        │  flagged events only
        ▼
LAYER 1 — YARA SIGNATURES        sig_engine.c    millisecond
LAYER 2 — eBPF BEHAVIORAL RULES  bpf/*.bpf.c     real-time
LAYER 3 — FILE INTEGRITY (FIM)   fim.c           SHA-256 chain
LAYER 4 — ANOMALY ML             ml/anomaly.py   isolation forest (~20 MB)
LAYER 5 — NETWORK IDS            (phase 2)
        │
        ▼
ENFORCEMENT TIER                 enforce.c
  reversible (freeze/quarantine/block-net) → autonomous
  irreversible (kill/delete/kill-switch)   → HUMAN CONFIRM REQUIRED
        │
        ▼
TAMPER-EVIDENT LOG               guardian.c      chain-hashed, append-only
```

## Project layout

```
guardian/
├── activate.sh          # source this to set up the toolchain env
├── Makefile             # make {bpf|sigs|guardian|ml|all|test|clean}
├── include/guardian.h   # common types, event bus API
├── bpf/                 # eBPF behavioral-rule programs (.bpf.c → .bpf.o)
│   └── trace_execve.bpf.c
├── src/                 # the daemon (C)
│   ├── guardian.c       # main, event bus, tamper-evident log
│   ├── sig_engine.c     # YARA Layer 1
│   ├── fim.c            # File Integrity Layer 3 (custom, replaces AIDE)
│   └── enforce.c        # enforcement tier policy
├── ml/anomaly.py        # isolation-forest anomaly model (Layer 4)
├── rules/guardian.yar   # YARA signature rules
├── mcp/mcp_server.py    # deep-kernel MCP (~50 tools, JSON-RPC)
├── docs/DEV_ENV.md      # what's installed, what runs here vs target
└── tests/
```

## Quick start

```bash
cd /home/z/guardian
source activate.sh          # sets up clang/libbpf/yara env
make all                    # build everything
./build/guardian --self-test
```

## What works in this dev sandbox vs WLTIOS target

| Capability            | Dev sandbox           | WLTIOS target |
|-----------------------|-----------------------|---------------|
| Compile eBPF programs | ✅                    | ✅            |
| Load/attach eBPF      | ❌ (no CAP_BPF)       | ✅            |
| YARA scan             | ✅                    | ✅            |
| FIM                   | ✅ (limited paths)    | ✅            |
| Anomaly ML (train)    | ✅                    | ✅            |
| Enforcement actions   | simulated (no root)   | ✅            |
| vmlinux.h generation  | ❌ (needs bpftool)    | ✅ (on target)|

See `docs/DEV_ENV.md` for the full inventory.

## RAM budget (target)

- Idle (all 5 layers running): ~100–170 MB
- During incident (Qwythos spins up): +6 GB, then spins back down to 0
- <4 GB RAM targets: Qwen2.5-1.5B as consultant fallback, or pure Defender-style (no AI)

## Decisions locked in

- **Defender-style core**, not LLM-first. The LLM is an optional consultant.
- **VibeThinker dropped** — not trained for tool-calling; wrong role for security.
- **Custom FIM** instead of AIDE (AIDE pulls too many deps for a minimal OS).
- **eBPF** for behavioral rules (no kernel modules — critical for WLTIOS).
- **Hybrid signatures**: frozen baseline on USB + Tor-fetched fresh rules.
- **Irreversible actions require human confirm** (architectural anti-hallucination).
