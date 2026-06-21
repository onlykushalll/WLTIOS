# ADR-05: Wayland + wlroots for Display

**Date:** 2026-06-21
**Status:** Accepted
**Research:** docs/context/RESEARCH_TAILS_WLROOTS.md

## Context

WLTIOS needs a display system for the GUI. Options:

1. **X11** — Legacy display server (1980s design). Still widely used but being replaced.
2. **Wayland (custom from scratch)** — Modern protocol, but building a compositor from scratch is enormous.
3. **Wayland + wlroots** — Modern protocol + library that handles the hard parts.

## Decision

**Use Wayland with wlroots as the compositor library. Build WLT-Compositor on top of wlroots.**

## Consequences

### Positive
- **Modern** — Wayland is the future. X11 is in maintenance mode.
- **wlroots handles the hard parts** — GPU, input, output, rendering. We focus on window management + UI.
- **Reference code** — `tinywl.c` (913 lines) is a minimal compositor we can learn from.
- **Hardware acceleration** — wlroots supports OpenGL/Vulkan rendering.
- **Lower RAM** — Wayland uses ~50MB less than X11.
- **Security** — Wayland isolates apps from each other (X11 doesn't).
- **Performance** — 60fps compositing with damage tracking.
- **Multi-monitor** — wlroots handles multiple outputs natively.

### Negative
- **Some apps need X11** — We'll need XWayland for legacy apps (browsers, etc.)
- **wlroots is a dependency** — Must build + maintain it. Updates may break our compositor.
- **Learning curve** — Wayland protocol + wlroots API is complex.

## What We Build (Custom)

| Component | Built On | Purpose |
|-----------|---------|---------|
| WLT-Compositor | wlroots | Window management, compositing |
| WLT-Render | wlroots renderer | Custom shadows, blur, animations |
| WLT-Shell | WLT-Compositor | Taskbar, launcher, notifications |
| WLT-Widgets | WLT-Render | Custom widget toolkit |
| WLT-Settings | WLT-Widgets | Settings app |
| WLT-FileManager | WLT-Widgets | File manager |
| WLT-Terminal | WLT-Widgets | Terminal emulator |

## What We Reuse (from wlroots)

| wlroots Feature | What It Does |
|----------------|-------------|
| Backend | Talks to DRM/KMS (kernel display) |
| Renderer | OpenGL/Vulkan rendering |
| Output | Monitor management |
| Input | Keyboard, mouse, touch input |
| xdg-shell | Desktop app window management |
| layer-shell | Panels, overlays, backgrounds |
| XWayland | X11 app compatibility |

## Alternatives Considered

### Alternative A: X11
- **Pros:** Mature, every app works
- **Cons:** Insecure (any app can keylog), bloated, deprecated
- **Rejected because:** X11 is legacy. Security model is broken.

### Alternative B: Custom display protocol (not Wayland)
- **Pros:** Maximum control
- **Cons:** Reinventing the wheel. No app compatibility. Years of work.
- **Rejected because:** Wayland + wlroots gives us 90% of what we need for free.

### Alternative C: Use GNOME/KDE compositor
- **Pros:** Done for us
- **Cons:** Not custom. Can't control the UX. Heavy (GNOME uses 500MB+).
- **Rejected because:** WLTIOS's value is a custom, lightweight GUI.

## References

- wlroots: https://gitlab.freedesktop.org/wlroots/wlroots
- tinywl.c reference: https://github.com/swaywm/wlroots/blob/master/tinywl/tinywl.c
- Wayland protocol: https://wayland.freedesktop.org/
- Research: docs/context/RESEARCH_TAILS_WLROOTS.md

---

**Decision made:** 2026-06-21
**Revisit if:** wlroots project becomes unmaintained (unlikely)
