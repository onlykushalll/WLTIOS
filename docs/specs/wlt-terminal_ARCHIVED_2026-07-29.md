# Spec: WLT-Terminal (Terminal Emulator)

> **Status:** Draft (ready for implementation)
> **Phase:** 1 (Month 4)
> **Dependencies:** WLT-Compositor, WLT-Widgets
> **ADR references:** ADR-05 (Wayland + wlroots)

## Purpose

WLT-Terminal is the terminal emulator — how users access the shell. It renders text, handles keyboard input, and connects to a shell process (bash or custom).

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Keyboard input | WLT-Compositor | Key events |
| Shell output | PTY (pseudo-terminal) | Text + ANSI escape codes |
| Config | `/etc/wlt/terminal.conf` | INI-style text |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Rendered text | WLT-Compositor (surface) | Pixels |
| Keyboard input | PTY (shell) | Characters |
| Window | WLT-Compositor | Wayland surface |

## Dependencies

- **WLT-Compositor** (Wayland surface)
- **WLT-Widgets** (scrollbar, if used)
- **musl libc** (openpty, fork, exec)
- **Custom shell** (or bash)

## What Depends on This

- User (primary interface for power users)

## Error Handling

| Error | Action |
|-------|--------|
| PTY allocation fails | Show error, close terminal |
| Shell process dies | Show "shell exited", option to restart |
| Font loading fails | Fall back to bitmap font |
| Terminal buffer full | Scroll (discard oldest lines) |

## Tests

| Test | How |
|------|-----|
| Text output | Run `echo hello`, verify "hello" appears |
| ANSI colors | Run `echo -e '\e[31mred\e[0m'`, verify red text |
| Keyboard input | Type `ls`, verify `ls` appears + output shows |
| Resize | Resize window, verify terminal reflows |
| Scroll | Output many lines, verify scrollback works |
| Copy/paste | Select text, copy, paste, verify correct |
| Tab support | Open multiple tabs, verify switching works |

## Security

- **Runs as user** (not root) — Terminal itself has no special privileges
- **Shell sandbox** — The shell process runs in a sandbox (like all apps)
- **No clipboard history** — Clipboard cleared when terminal closes

## Performance

| Metric | Budget |
|--------|--------|
| Text render | < 5ms per frame |
| Input latency | < 16ms |
| Scrollback | 10,000 lines |
| RAM | < 10MB |

## File Structure

```
src/userspace/wlt-terminal/
├── include/
│   ├── terminal.h        # Main terminal struct
│   ├── pty.h             # PTY management
│   ├── render.h          # Text rendering
│   ├── input.h           # Keyboard handling
│   ├── ansi.h            # ANSI escape code parser
│   ├── font.h            # Font loading + rendering
│   └── config.h          # Config parsing
├── terminal.c            # Main terminal logic
├── pty.c                 # PTY open/close, I/O
├── render.c              # Glyph rendering
├── input.c               # Keyboard → PTY
├── ansi.c                # ANSI escape code parser
├── font.c                # TTF/OTF font loading (stb_truetype)
├── scrollback.c          # Scrollback buffer
├── config.c              # Config parsing
└── Makefile
```

## Config Format

```ini
[wlt-terminal]
font = monospace
font_size = 14
color_scheme = dark       # dark | light | custom
scrollback_lines = 10000
cursor_style = block      # block | underline | bar
bell = none               # none | sound | visual
background_opacity = 1.0  # 1.0 = opaque, 0.5 = semi-transparent

[colors]
background = #1a1b26
foreground = #c0caf5
cursor = #c0caf5
selection = #33467c
# Normal colors (0-7)
color0 = #15161e
color1 = #f7768e
color2 = #9ece6a
color3 = #e0af68
color4 = #7aa2f7
color5 = #bb9af7
color6 = #7dcfff
color7 = #a9b1d6
# Bright colors (8-15)
color8 = #414868
color9 = #f7768e
color10 = #9ece6a
color11 = #e0af68
color12 = #7aa2f7
color13 = #bb9af7
color14 = #7dcfff
color15 = #c0caf5
```

---

**Spec version:** 1.0
