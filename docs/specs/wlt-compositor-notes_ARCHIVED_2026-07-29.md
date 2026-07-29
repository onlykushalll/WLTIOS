# Spec: WLT-Compositor — Implementation Notes

> **Status:** Ready for implementation
> **Phase:** 2 (Month 7-8)
> **Dependencies:** wlroots (must be built first)

## Implementation Strategy

WLT-Compositor is built on wlroots. Before writing compositor code, we need:

1. **Build wlroots** on Linux (needs meson + wayland-protocols + Mesa)
2. **Study tinywl.c** — the reference minimal compositor (913 lines)
3. **Build our compositor** on top of wlroots

## tinywl.c Reference

The wlroots project ships `tinywl/tinywl.c` — a minimal but functional compositor. Our WLT-Compositor will start as a fork of tinywl, then add:

- Custom window management (snap, tile, etc.)
- Custom rendering (shadows, blur, animations)
- layer-shell support (for taskbar)
- IPC socket (for WLT-Shell communication)
- Config file parsing

## Key wlroots APIs

```c
// Backend (DRM/KMS or X11 for testing)
struct wlr_backend *backend = wlr_autocreate(wl_display);

// Renderer (OpenGL ES 2.0)
struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);

// Allocator (GBM or shm)
struct wlr_allocator *allocator = wlr_allocator_autocreate(backend, renderer);

// Scene graph (newer wlroots API — simpler than manual rendering)
struct wlr_scene *scene = wlr_scene_create();

// Output layout (multi-monitor)
struct wlr_output_layout *layout = wlr_output_layout_create();

// Seat (input)
struct wlr_seat *seat = wlr_seat_create(wl_display, "seat0");

// Cursor
struct wlr_cursor *cursor = wlr_cursor_create();
struct wlr_xcursor_manager *xcursor = wlr_xcursor_manager_create(NULL, 24);

// xdg-shell (desktop apps)
struct wlr_xdg_shell *xdg = wlr_xdg_shell_create(wl_display, 2);

// layer-shell (panels, taskbar)
struct wlr_layer_shell *layer = wlr_layer_shell_v1_create(wl_display, 3);
```

## Build Dependencies (Linux)

```
meson
ninja
wayland-protocols
wayland-utils
libxkbcommon
Mesa (OpenGL)
libdrm
gbm
pixman (for software rendering fallback)
```

## File Structure (Planned)

```
src/userspace/wlt-compositor/
├── include/
│   ├── compositor.h      # Main struct (server state)
│   ├── output.h          # Monitor management
│   ├── input.h           # Keyboard/mouse/touch
│   ├── view.h            # Window management
│   ├── render.h          # Custom rendering
│   ├── config.h          # Config parsing
│   └── ipc.h             # IPC to WLT-Shell
├── compositor.c          # Main entry + event loop
├── output.c              # Monitor detection + modes
├── keyboard.c            # Keyboard + keybindings
├── pointer.c             # Mouse + cursor
├── touch.c               # Touch input
├── view.c                # Window management
├── xdg.c                 # xdg-shell protocol
├── layer.c               # layer-shell protocol
├── render.c              # Frame rendering
├── config.c              # Config file parsing
├── ipc.c                 # IPC socket
└── meson.build           # Build file
```

## Next Steps

1. Get seL4 + Linux booting in QEMU (needs Linux build env)
2. Build wlroots on Linux
3. Study tinywl.c source
4. Fork tinywl → WLT-Compositor
5. Add custom features incrementally

---

**Note:** This compositor cannot be built on Windows. It needs Linux (wlroots depends on DRM/KMS, Mesa, wayland-protocols — all Linux-specific). Must use WSL2 or Linux VM for development.
