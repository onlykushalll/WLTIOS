# ADR-08: Kairos — WLTIOS AI Architecture

**Date:** 2026-06-22
**Status:** Accepted
**Spec:** docs/specs/wlt-ai.md (supersedes)

## Decision

WLTIOS's built-in AI is named **Kairos** — Knowledge Adaptive Intelligence Driven Reasoning Operating System.

### Identity
- **Name:** Kairos
- **Hotword:** "Wake up, Buddy!"
- **Personality:** Adaptive (combines generic + character + personalized)
  - Default: Protective, intelligent, proactive guardian (JARVIS-like)
  - Adapts to user's communication style over time
  - Serious when security matters, conversational when casual
- **Core values:** Privacy first, proactive protection, never leaks data, fights threats

### Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                        KAIROS AI                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─── INPUT LAYER ───────────────────────────────────────┐  │
│  │  Text (sidebar chat)                                   │  │
│  │  Voice ("Wake up, Buddy!" → STT → Whisper)            │  │
│  │  Screen (VLM — sees what user sees, 2 FPS)            │  │
│  │  System events (Tor disconnect, app crash, intrusion)  │  │
│  │  Network traffic (anomaly detection via Firewall PD)   │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── BRAIN LAYER (Router) ──────────────────────────────┐  │
│  │  Simple query?  → Local model (Llama 3 8B, <1GB RAM)  │  │
│  │  Complex task?  → Cloud model (user's API key)        │  │
│  │  Security alert? → Instant local response (<100ms)    │  │
│  │  Screen analysis? → VLM (local or cloud)              │  │
│  │  No cloud = no data leaves device (Tails-level privacy)│  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── LOCAL MODEL (Privacy-First) ───────────────────────┐  │
│  │  Llama 3 8B (quantized 4-bit, ~4.5GB RAM)             │  │
│  │  Phi-3-mini (fallback, ~1.8GB RAM)                    │  │
│  │  Whisper-small STT (~500MB RAM)                       │  │
│  │  Coqui TTS (~200MB RAM)                               │  │
│  │  Total local AI RAM: <2GB (models loaded on demand)   │  │
│  │  Idle RAM: <100MB (models unloaded when not active)   │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── CLOUD MODELS (User API Keys, Privacy-Safe) ───────┐  │
│  │  Supported: Gemini, Claude, GLM, OpenAI, any OAI-compat│  │
│  │  Privacy pipeline:                                     │  │
│  │    1. Strip PII from prompt (names, emails, IPs)       │  │
│  │    2. Route through Tor (no IP leak)                   │  │
│  │    3. No conversation history sent (only current msg)  │  │
│  │    4. Response sanitized before display                │  │
│  │    5. API key stored in Key Vault PD (encrypted)       │  │
│  │    6. User sees exactly what's sent (transparent log)  │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── MCP TOOL INTEGRATION ──────────────────────────────┐  │
│  │  Browser control (navigate, click, type, scrape)       │  │
│  │  File operations (search, read, write, encrypt)        │  │
│  │  System control (open apps, settings, kill switch)     │  │
│  │  Network tools (scan, Tor check, DNS leak test)        │  │
│  │  Shell execution (sandboxed, denylist-protected)        │  │
│  │  Screen capture + analysis (VLM)                       │  │
│  │  App management (install, remove, permission audit)    │  │
│  │  External MCP servers (user-configurable)              │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── PROACTIVE GUARDIAN (JARVIS Mode) ─────────────────┐  │
│  │  Runs 24/7 (even when user inactive):                  │  │
│  │  • Network intrusion detection (anomaly scoring)       │  │
│  │  • Port scan detection (blocks + alerts)               │  │
│  │  • Malware behavior detection (CPU/RAM/Disk anomalies) │  │
│  │  • Tor circuit health monitoring                       │  │
│  │  • DNS leak detection (continuous)                     │  │
│  │  • App permission violations (auto-revoke)             │  │
│  │  • Phishing URL detection (browser integration)        │  │
│  │  • Data exfiltration detection (unusual uploads)       │  │
│  │  • Auto-kill-switch on attack detection                │  │
│  │  • Forensic logging (encrypted, tamper-proof)          │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── MEMORY ────────────────────────────────────────────┐  │
│  │  Short-term: Current session context (<1MB)            │  │
│  │  Long-term: Encrypted conversation history (AES-256)   │  │
│  │  Amnesic: Wipe everything on reboot (Live USB mode)    │  │
│  │  Selective: User can pin memories that persist         │  │
│  │  No memory sent to cloud (only current message)        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌─── OUTPUT LAYER ──────────────────────────────────────┐  │
│  │  Text (sidebar chat, system notifications)             │  │
│  │  Voice (TTS — natural speech, interruptible)           │  │
│  │  Visual (screen highlights, overlay arrows, alerts)    │  │
│  │  Action (directly executes — opens apps, changes config)│  │
│  └──────────────────────────────────────────────────────┘  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### RAM Budget

| Component | Idle | Active |
|-----------|------|--------|
| Kairos daemon | 50MB | 100MB |
| Local LLM (Llama 3 8B 4-bit) | 0MB (unloaded) | 4.5GB |
| Local LLM (Phi-3-mini) | 0MB (unloaded) | 1.8GB |
| STT (Whisper-small) | 0MB (unloaded) | 500MB |
| TTS (Coqui) | 0MB (unloaded) | 200MB |
| VLM (screen analysis) | 0MB (unloaded) | 1GB |
| Guardian monitor | 20MB | 20MB |
| **Total idle** | **70MB** | — |
| **Total active (minimal)** | — | **700MB** (Phi-3 only) |
| **Total active (full)** | — | **6.3GB** (all models) |

Models are loaded on-demand and unloaded after 30s of inactivity.

### Privacy Pipeline for Cloud Models

```
User asks: "What's in this email?"
        │
        ▼
┌─── KAIROS PRIVACY PIPELINE ───┐
│                               │
│  1. PII STRIPPER              │ ← Removes names, emails, IPs,
│     "What's in this email?"   │   phone numbers, addresses
│     → "What's in this [MSG]?" │   Replaces with [NAME], [EMAIL]
│                               │
│  2. CONTEXT FILTER            │ ← Removes conversation history
│     Only sends current msg    │   Never sends past messages
│                               │
│  3. TOR ROUTING               │ ← Request goes through Tor
│     → 127.0.0.1:9050         │   No IP leak
│     → Cloud API               │   ISP can't see the request
│                               │
│  4. RESPONSE SANITIZER        │ ← Checks response for:
│     ← Cloud response          │   - Tracking pixels/URLs
│     → Clean response          │   - Suspicious links
│     → Display to user         │   - Data collection scripts
│                               │
│  5. TRANSPARENCY LOG          │ ← User can see exactly:
│     "Sent to [provider]:      │   - What was sent
│      'What's in this [MSG]?'" │   - To which provider
│     User can review anytime   │   - What was received
│                               │
└───────────────────────────────┘
```

### MCP Tool Framework

Kairos has built-in MCP (Model Context Protocol) tool integration:

```json
{
  "tools": [
    {
      "name": "browser_navigate",
      "description": "Navigate browser to URL (through Tor)",
      "params": {"url": "string"},
      "safety": "url_checked_against_phishing_db"
    },
    {
      "name": "browser_scrape",
      "description": "Extract text content from current page",
      "params": {"selector": "string?"},
      "safety": "no_js_execution"
    },
    {
      "name": "file_search",
      "description": "Search files by description",
      "params": {"query": "string", "path": "string?"},
      "safety": "respects_app_sandbox"
    },
    {
      "name": "system_control",
      "description": "Control OS (open app, change setting)",
      "params": {"action": "string", "args": "object"},
      "safety": "denylist_protected"
    },
    {
      "name": "security_scan",
      "description": "Run security audit on system",
      "params": {"scope": "string"},
      "safety": "read_only"
    },
    {
      "name": "tor_check",
      "description": "Check Tor circuit health + DNS leaks",
      "params": {},
      "safety": "read_only"
    },
    {
      "name": "screen_analyze",
      "description": "Analyze current screen with VLM",
      "params": {"question": "string"},
      "safety": "local_only"
    },
    {
      "name": "shell_exec",
      "description": "Execute shell command (sandboxed)",
      "params": {"command": "string"},
      "safety": "denylist + sandbox + user_confirm"
    }
  ]
}
```

### Proactive Guardian (JARVIS Mode)

Kairos runs a background guardian that protects the user 24/7:

```
┌─── GUARDIAN EVENT LOOP (every 1 second) ───────────┐
│                                                    │
│  CHECK: Network anomalies                          │
│    → Unusual port scans? → Block + alert user      │
│    → Unexpected connections? → Kill + alert        │
│    → Bandwidth spike? → Check for data exfil       │
│                                                    │
│  CHECK: Tor health                                 │
│    → Tor disconnected? → Trigger kill switch       │
│    → DNS leak detected? → Alert + fix              │
│    → Circuit degraded? → New identity              │
│                                                    │
│  CHECK: Process behavior                           │
│    → CPU spike from unknown process? → Investigate │
│    → RAM hog? → Alert + suggest kill               │
│    → Process accessing camera without perm? → Kill │
│    → Process writing to /etc? → Block + alert      │
│                                                    │
│  CHECK: App permissions                            │
│    → App bypassing sandbox? → Revoke + alert       │
│    → App accessing files outside scope? → Block    │
│    → App making direct network (not Tor)? → Block  │
│                                                    │
│  CHECK: Browser safety                             │
│    → Phishing URL? → Block + warn user             │
│    → Malicious download? → Quarantine + scan       │
│    → Tracking scripts? → Block + report            │
│                                                    │
│  CHECK: Disk security                              │
│    → Disk encryption disabled? → Alert             │
│    → Sensitive file unencrypted? → Offer encrypt   │
│    → Unattended screen? → Auto-lock after timeout  │
│                                                    │
│  RESPONSE:                                         │
│    → Low threat: Log + silent fix                  │
│    → Medium threat: Notify user (notification)      │
│    → High threat: Alert user (voice + visual)       │
│    → Critical threat: Kill switch + lockdown        │
│                                                    │
└────────────────────────────────────────────────────┘
```

### API Key Management

Users can add their own API keys for cloud models:

```ini
# /etc/wlt/kairos-providers.conf
[providers]
# Keys stored encrypted in Key Vault PD, referenced here by ID
gemini = keyvault:gemini_api_key
claude = keyvault:claude_api_key
openai = keyvault:openai_api_key
glm = keyvault:glm_api_key
# Any OpenAI-compatible endpoint
custom = keyvault:custom_api_key
custom_url = https://api.custom-ai.com/v1

[routing]
# Which provider to use for which task
default = local          # Default: local model
complex = gemini         # Complex reasoning: Gemini
code = claude            # Code generation: Claude
vision = local           # Screen analysis: local VLM
creative = glm           # Creative writing: GLM

[privacy]
strip_pii = yes          # Remove PII before sending
route_through_tor = yes  # All cloud requests via Tor
no_history = yes         # Don't send conversation history
transparency_log = yes   # Log what's sent
```

## References

- WLT-AI code: src/userspace/wlt-ai/
- Spec: docs/specs/wlt-ai.md
- MCP protocol: https://modelcontextprotocol.io/
- Llama 3: https://llama.meta.com/
- Whisper: https://github.com/openai/whisper

---

**Decision made:** 2026-06-22
**Decision maker:** Kushal (named Kairos, defined hotword, personality, architecture)
