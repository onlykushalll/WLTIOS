# WLT-AI Orphaned Code Audit (2026-07-20)

## Method
`wc -l` on all .c files, `gcc -fsyntax-only` compile check, grep for
TODO/stubs/placeholders, read first 30 + last 20 lines of each file.

## Summary Table

| File | Lines | Compiles | Stubs? | What it actually does |
|------|-------|----------|--------|----------------------|
| sidebar.c | 183 | ✅ | render is stub | Chat UI state (msg history, input, visibility) — render returns OK without drawing |
| vision.c | 579 | ✅ | curl stubs | Screen capture (fbdev/X11/Wayland) + image analysis via HTTP — uses curl_compat.h stubs |
| voice.c | 188 | ✅ | none | STT/TTS wrapper — calls ai.c's stt/tts functions (which are curl stubs) |
| hotword.c | 144 | ✅ | placeholder | Energy-based voice activity detection (placeholder for real KWS model) |
| anonymity_coach.c | 124 | ✅ | none | Monitors Tor status, DNS leaks, kill switch, MAC randomization — REAL logic |
| privacy_analysis.c | 170 | ✅ | none | App permission risk assessment (safe/low/medium/high/critical) — REAL logic |
| privacy_pipeline.c | 261 | ✅ | none | PII stripping (regex) + Tor routing for cloud AI — REAL logic |
| cloud_api.c | 176 | ✅ | none | Cloud LLM provider config (Gemini/Claude/GLM) — REAL config management |
| local_model.c | 140 | ✅ | void *ctx | LLM model state — has `void *ctx` opaque pointer, NOT connected to llama.cpp |
| proactive_protection.c | 164 | ✅ | none | JARVIS-style defense: port scan, brute force, suspicious process detection — REAL |
| visual_feedback.c | 103 | ✅ | none | Overlay state (highlight/arrow/dim/pulse) — in-memory only, no rendering |
| kairos_mcp.c | 484 | ✅ | 1 TODO | MCP tool-use: URL analysis, phishing detection, app control — REAL |
| guardian.c | 342 | ✅ | 17 TODOs | Guardian ↔ Kairos bridge: monitors FIM, network, processes — TODOs are future integration |

## Key Findings

### 1. sidebar.c — render function is a stub
```c
wlt_ai_err_t wlt_sidebar_render(void *render_ctx) {
    (void)render_ctx;
    if (!g_sidebar.visible) return WLT_AI_OK;
    return WLT_AI_OK;  // ← does nothing
}
```
The message history, input handling, and visibility state are all real. The
render function needs to be wired to a Wayland surface (like kairos_window.c).

**Decision for Section A**: kairos_window.c IS the sidebar — it's already a
Wayland client with keyboard handling and chat display. sidebar.c provides the
data model (message types, visibility, status indicator) but kairos_window.c
already implements the rendering. The spec says "a sidebar, always available" —
kairos_window.c achieves this as a persistent Wayland surface. sidebar.c's
render function should be wired to kairos_window.c's render path, not built as
a separate Wayland client.

### 2. anonymity_coach.c — REAL, no stubs
Tracks: tor_connected, dns_via_tor, kill_switch_active, mac_randomized,
webrtc_leak, active_circuits, exit_country, exit_ip. Generates warnings.
`wlt_ai_anon_is_safe()` returns a bool. Can be wired to kairos_window.c
directly — call `wlt_ai_anon_check()` on a timer, display warnings in the
sidebar.

### 3. privacy_analysis.c — REAL, no stubs
App risk assessment: `wlt_ai_privacy_add_app()` adds an app with its
permissions, `calculate_risk()` computes risk level. Can be wired to
wlt-permissions (already verified real) — read app manifests, call
`wlt_ai_privacy_add_app()`, display risk levels in the sidebar.

### 4. proactive_protection.c — REAL, no stubs
Event-driven defense: port scans, brute force, suspicious processes, disk
encryption off, Tor disconnected, new USB device, user idle, screen lock off.
Can be wired to guardian's socket (guardian.conf has socket_path).

### 5. privacy_pipeline.c — REAL, no stubs
Regex-based PII stripping (emails, IPs, phone numbers, SSNs). Routes through
Tor. Logs what was sent. Can be wired to cloud_api.c for cloud LLM queries.

### 6. guardian.c — 17 TODOs, all "TODO(kairos):"
All TODOs are for future Kairos integration (send alerts to user, trigger
kill switch, voice alerts). The monitoring logic (check_disk_encryption,
check_network_connections, check_processes, check_app_permissions) is REAL.

### 7. local_model.c — void *ctx (not connected)
Has a `wlt_lm_state_t` struct with `void *ctx` for the llama.cpp context.
The model loading and inference functions are stubs — they set state but
don't call any real llama.cpp functions. This is intentionally disconnected
(respects the "no models on RAM" constraint).

### 8. hotword.c — placeholder detection
Uses simple energy-based detection (RMS threshold) instead of a real
keyword spotting model. This is a hardware-dependent feature that can't
be tested in headless QEMU.
