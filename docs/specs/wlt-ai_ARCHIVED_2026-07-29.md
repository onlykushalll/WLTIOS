# Spec: WLT-AI (AI Assistant)

> **Status:** Draft (ready for implementation)
> **Phase:** 4 (Month 19-20)
> **Dependencies:** WLT-Compositor, WLT-Widgets, VLM, LLM
> **ADR references:** None (unique to WLTIOS)

## Purpose

WLT-AI is the built-in AI assistant. It can see the screen (VLM), hear voice (STT), speak responses (TTS), control the OS (open apps, change settings), and answer questions. It runs locally by default — no cloud required.

## What Makes This Unique

**No other OS has AI natively integrated.** Not Windows (Copilot is an app), not macOS (Apple Intelligence is limited), not Linux (no integration). WLT-AI is a first-class OS citizen.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Screen capture | WLT-Compositor | Framebuffer (PNG) |
| Voice input | Microphone (via WLT-Sandbox permission) | Audio stream |
| Text input | User (via sidebar) | Text |
| System state | WLT-Init (IPC) | Service status |
| File access | WLT-FileManager (IPC) | File contents |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Text response | WLT-Shell (sidebar) | Text |
| Voice response | Speakers | Audio stream |
| System actions | Various modules (IPC) | Commands (open app, change setting) |
| File searches | WLT-FileManager (IPC) | Search results |

## Dependencies

- **WLT-Compositor** (screen capture for VLM)
- **WLT-Shell** (sidebar UI)
- **WLT-Sandbox** (AI runs sandboxed like all apps)
- **Local models:** Llama-Vision (VLM), Whisper (STT), Coqui TTS
- **Optional cloud:** Gemini, Claude, GLM (user enables)

## What Depends on This

- User (primary AI interaction)

## AI Capabilities

| Capability | How |
|-----------|-----|
| **Screen understanding** | VLM analyzes screenshots — "what's on my screen?" |
| **Voice commands** | STT transcribes speech → LLM processes → action |
| **Voice responses** | TTS reads responses aloud |
| **System control** | "Open Firefox" → launches app. "Turn on kill switch" → triggers it. |
| **File search** | "Find the PDF I downloaded last week" → searches files |
| **Privacy analysis** | "Which apps are spying?" → analyzes app permissions + network |
| **Anonymity coach** | "You're leaking DNS" → detects + warns |
| **Code assistance** | Built-in code editor integration |
| **Help** | "How do I change my MAC address?" → step-by-step guide |

## Privacy

- **Runs locally by default** — No data leaves the device
- **Optional cloud** — User explicitly enables cloud models (Gemini/Claude/GLM)
- **No training on user data** — Local models don't train
- **AI memory encrypted** — Conversation history encrypted at rest
- **Amnesic option** — Can forget all conversations on reboot
- **Sandboxed** — AI runs in its own sandbox, no special privileges

## Performance

| Metric | Budget |
|--------|--------|
| Screen analysis (VLM) | < 2 seconds per frame |
| Voice command (STT + LLM) | < 3 seconds |
| Voice response (TTS) | < 1 second to start |
| Text response | < 1 second |
| RAM (idle) | < 100MB |
| RAM (active) | < 500MB |

## File Structure

```
src/userspace/wlt-ai/
├── Cargo.toml
├── src/
│   ├── main.rs            # AI assistant daemon
│   ├── core/
│   │   ├── mod.rs
│   │   ├── llm.rs         # LLM integration (local + cloud)
│   │   ├── vlm.rs         # Vision model (screen understanding)
│   │   ├── stt.rs         # Speech-to-text
│   │   ├── tts.rs         # Text-to-speech
│   │   ├── tools.rs       # Tool use (system control)
│   │   └── memory.rs      # Conversation history (encrypted)
│   ├── ui/
│   │   ├── sidebar.rs     # Sidebar UI
│   │   ├── voice.rs       # Voice indicator
│   │   └── notifications.rs
│   ├── ipc/
│   │   ├── compositor.rs  # Screen capture
│   │   ├── shell.rs       # System actions
│   │   ├── files.rs       # File search
│   │   └── init.rs        # System state
│   ├── models/
│   │   ├── local.rs       # Local model loading (llama.cpp)
│   │   └── cloud.rs       # Cloud API clients (optional)
│   └── error.rs
└── tests/
```

## Config Format

```ini
[wlt-ai]
enabled = yes
mode = local             # local | cloud | hybrid
hotword = "hey wltios"   # Voice activation hotword
voice_output = yes       # Speak responses aloud
memory = amnesic         # amnesic | persistent

[models]
vlm = llama-vision-7b    # Local VLM model
llm = llama-3-8b         # Local LLM model
stt = whisper-small      # Local STT model
tts = coqui-tts          # Local TTS engine

[cloud]                  # Only if mode = cloud or hybrid
provider = none          # none | gemini | claude | glm
api_key =                # Read from Key Vault (never hardcoded)
```

---

**Spec version:** 1.0
