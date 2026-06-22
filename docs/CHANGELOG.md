# Changelog

All notable changes to WLTIOS are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added — 2026-06-22 (Session 4)

#### Kairos AI Modules (10 new files)
- `wlt-ai/hotword.c` — "Wake up, Buddy!" wake word detection (energy-based KWS, pthread background)
- `wlt-ai/sidebar.c` — AI chat sidebar UI (message queue, input handling, show/hide/toggle)
- `wlt-ai/voice.c` — TTS (Piper/espeak queue) + STT (whisper.cpp continuous listening)
- `wlt-ai/visual_feedback.c` — Screen overlays (highlight, pulse, dim, arrow)
- `wlt-ai/file_search.c` — Natural language file search ("find my PDF from Monday")
- `wlt-ai/privacy_analysis.c` — App privacy risk scoring (safe/low/medium/high/critical)
- `wlt-ai/anonymity_coach.c` — Anonymity health monitor (DNS leak, Tor, MAC, WebRTC checks)
- `wlt-ai/proactive_protection.c` — Active defense (idle detection, attack alerts, event logging)
- `wlt-ai/local_model.c` — Local LLM integration (VibeThinker-3B, Qwen2.5-3B, Llama 3.2 via llama.cpp)
- `wlt-ai/cloud_api.c` — Cloud API plugin (Gemini/Claude/GLM, privacy filter, Tor routing)

#### Tor Browser
- `wlt-tor-browser/browser.c` — Launcher with Tor SOCKS proxy check
- `wlt-tor-browser/prefs.js` — Maximum privacy Firefox config (WebRTC off, JS off, fingerprinting resist)

#### Testing
- `tests/integration/pentest.sh` — 8-test penetration testing suite (non-Tor blocked, DNS leak, kill switch, MAC, encryption, ports, boot verify, amnesic)

#### Infrastructure
- `config/repo/repo-manifest.json` — Package repository manifest (8 packages)
- `docs/community/website-content.md` — Website content

#### Documentation
- `docs/PRIVACY.md` — Privacy policy (zero data collection, Tor routing, amnesic mode)
- `docs/SECURITY.md` — Security policy (vulnerability reporting, architecture summary)
- `docs/CODE_OF_CONDUCT.md` — Community code of conduct
- `docs/CHANGELOG.md` — This file
- `docs/FILETREE.md` — Complete file tree for final product + honest 5-8% assessment
- `docs/user-guide/getting-started.md` — End-user guide (installation, desktop, Tor, Kairos, troubleshooting)
- `docs/dev-guide/build-from-source.md` — Developer guide (build env, wlroots, seL4, testing, architecture)
- `docs/security/security-whitepaper.md` — Security architecture, threat model, key management
- `docs/security/audit-results.md` — Static analysis results (15 issues found + fixed)

### Changed
- `wlt-compositor/compositor.c` — Replaced stub with real wlroots 0.18 initialization (wl_display_create, wlr_backend_autocreate, wlr_renderer_autocreate, wlr_allocator_autocreate, wlr_scene_create, wlr_output_layout_create, wlr_seat_create)
- `wlt-compositor/include/compositor.h` — Version 2.0.0, added void* wlroots opaque pointers to state struct
- `wlt-compositor/compositor_test.c` — Added wlroots header compile-time check
- `wlt-crypto/keyvault.c` — Rewrote ALL function signatures to match keyvault.h exactly
- `wlt-pkg/verify.c` — Fixed undeclared variable `i` (changed to `for (int i=0;...)`)
- `wlt-pkg/dependency.c` — Added forward declaration of `g_db_count_hack`
- `wlt-pkg/store.c` — Added missing `#include <errno.h>`
- `wlt-sandbox/launch.c` — Added `_GNU_SOURCE` guard for `pipe2()`
- `wlt-tor-mgr/tor_vm.c` — Added socket includes (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`)
- `wlt-net/net.c` — Wrapped CLI section with `#ifndef WLT_TESTING`
- `wlt-tor-mgr/tor_mgr.c` — Wrapped main() with `#ifndef WLT_TESTING`
- `wlt-ai/guardian.c` — Added socket includes
- `wlt-ai/kairos_mcp.c` — Added socket includes
- `boot.c` — Fixed unused parameter `image_handle`
- `loader.c` — Fixed unknown type `CHAR16`
- `MODULE_MAP.md` — Updated compositor to ✅ (real wlroots 0.18)
- `ADR-04` — Amended to document actual CAmkES usage + Microkit transition plan
- `docs/specs/boot-verifier-pd.md` — Removed impossible `files.c`, clarified memory-only verification
- `docs/specs/key-vault-pd.md` — Added Key Persistence Architecture section (TPM-sealed bootstrap)
- `README.md` — Complete rewrite with professional structure

### Removed
- `tests/stubs/` — No longer needed (all modules compile against real source)
- `src/userspace/wlt-widgets/_test.txt` — Junk artifact

### Verified
- seL4 + Linux boots in QEMU (32-bit, `-cpu host`, KVM) — `=== WLTIOS Boot ===` confirmed in serial output
- WLT-Shell runs as PID 1 inside seL4's Linux guest
- 88 unit test assertions pass (33 Day 1 + 55 Day 2)
- All 11 modules compile against real source with `-Wall -Wextra -Werror`
- wlroots 0.18.2 built and installed in WSL2
- Compositor compiles + links against wlroots 0.18
- Static security audit: 15 issues found + fixed, 0 vulnerabilities remaining

## [0.1.0] — 2026-06-21

### Added
- seL4 + Linux VM architecture (proven in QEMU)
- 24 modules with code (boot, init, shell, terminal, net, crypto, sandbox, pkg, etc.)
- 18 specs, 8 ADRs, 4 research reports
- Engineering framework (10 rules preventing vibe-coding traps)
- 41 unit tests (SHA-256 NIST verified, config parser, service parser, dependency resolver, monitor, ELF parser)
- seL4 builds: 32-bit (boots in QEMU) + 64-bit (ready for bare metal)
- WSL2 Ubuntu build environment
- QEMU 11.0.50 installed
- Git repo: github.com/onlykushalll/WLTIOS
