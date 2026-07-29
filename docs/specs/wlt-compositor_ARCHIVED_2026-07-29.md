# Spec: WLT-Compositor (Wayland Compositor)

> **Status:** Draft (ready for implementation)
> **Phase:** 2 (Month 7-8)
> **Dependencies:** wlroots, Linux kernel, musl libc
> **ADR references:** ADR-05 (Wayland + wlroots)

## Purpose

WLT-Compositor is the Wayland display compositor for WLTIOS. It manages windows, handles input (keyboard, mouse, touch), renders the desktop, and provides the foundation for all GUI applications.

## Inputs

| Input | Source | Format |
|-------|--------|--------|
| Display outputs | Linux DRM/KMS | Connected monitors + modes |
| Keyboard input | Linux evdev | Key events |
| Mouse/touch input | Linux evdev | Motion + button events |
| App surfaces | Wayland clients (via wlroots) | xdg-surface, layer-surface |
| Config | `/etc/wlt/compositor.conf` | INI-style text |

## Outputs

| Output | Destination | Format |
|--------|-------------|--------|
| Rendered frame | DRM/KMS (GPU) | Pixel buffer |
| Input events | Wayland clients | wl_keyboard, wl_pointer |
| Window events | WLT-Shell (via IPC) | JSON messages |
| Compositor status | `wltctl` | Text/JSON |

## Dependencies

- **wlroots** (Wayland library — handles DRM, rendering, input, protocols)
- **Linux DRM/KMS** (kernel display subsystem)
- **Mesa** (OpenGL/Vulkan drivers)
- **xkbcommon** (keyboard layout handling)
- **musl libc**

## What Depends on This

- WLT-Shell (taskbar, launcher — rendered as layer-shell surfaces)
- WLT-Widgets (renders on compositor surfaces)
- All GUI apps (browser, editor, file manager — run as Wayland clients)
- XWayland (for X11 app compatibility)

## Error Handling

| Error | Action |
|-------|--------|
| No display detected | Print error to serial console, halt |
| GPU driver missing | Fall back to software rendering (pixman) |
| wlroots init failure | Print error, halt |
| Client crash | Remove its surfaces, continue running |
| Out of memory for surface | Deny surface creation, send error to client |
| Config parse error | Use defaults, log warning |

## Tests

| Test | How |
|------|-----|
| Boot to blank desktop | Start compositor, verify DRM output |
| Window creation | Launch a test client, verify window appears |
| Window move | Drag window title bar, verify it moves |
| Window resize | Drag window edge, verify it resizes |
| Window close | Click close button, verify window disappears |
| Alt+Tab | Press Alt+Tab, verify window switcher appears |
| Multi-monitor | Connect 2 monitors, verify both show desktop |
| Client crash recovery | Kill a client, verify other windows unaffected |
| GPU fallback | Remove GPU driver, verify software rendering works |

## Security

- **App isolation:** Each Wayland client can only see its own surfaces. Cannot screenshot other apps. Cannot inject input into other apps.
- **No root:** Compositor runs as `wlt` user, not root.
- **Sandbox:** Compositor runs in a cgroup with memory limit (256MB).
- **Input isolation:** Keyboard/mouse events go only to the focused window (unless global hotkey).
- **Clipboard isolation:** Clipboard access requires client to be focused.

## Performance

| Metric | Budget |
|--------|--------|
| Frame rate | 60fps (16.67ms per frame) |
| Compositor RAM | < 32MB idle |
| Window open latency | < 50ms |
| Input latency | < 16ms (one frame) |
| Compositor binary size | < 500KB |

## Implementation Notes

- **Language:** C (wlroots is C)
- **Build:** Meson + Ninja (wlroots uses meson)
- **wlroots version:** Pin to a specific release (avoid breaking changes)
- **Renderer:** OpenGL ES 2.0 (fallback: pixman software renderer)

## File Structure

```
src/userspace/wlt-compositor/
├── include/
│   ├── compositor.h      # Main compositor struct
│   ├── output.h          # Output (monitor) management
│   ├── input.h           # Keyboard, mouse, touch
│   ├── view.h            # Window/surface management
│   ├── render.h          # Rendering pipeline
│   ├── config.h          # Config parsing
│   └── xdg.h             # xdg-shell handling
├── compositor.c          # Main entry + event loop
├── output.c              # Monitor detection + mode setting
├── input.c               # Input device handling
├── keyboard.c            # Keyboard + keybindings
├── pointer.c             # Mouse + cursor
├── touch.c               # Touch input
├── view.c                # Window management (move, resize, close)
├── xdg.c                 # xdg-shell protocol handling
├── layer.c               # layer-shell protocol (taskbar, launcher)
├── render.c              # Frame rendering
├── config.c              # Config file parsing
├── ipc.c                 # IPC to WLT-Shell
└── meson.build           # Build file
```

## Core Data Structures

```c
// The main compositor state
typedef struct {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;              // Scene graph API
    struct wlr_output_layout *output_layout;
    struct wl_list outputs;               // Linked list of outputs
    struct wl_list views;                 // Linked list of windows
    struct wlr_seat *seat;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_manager;
    struct wlr_keyboard_group *keyboard_group;
    struct wlr_xdg_shell *xdg_shell;
    struct wlr_layer_shell *layer_shell;
    // Config
    wlt_compositor_config_t config;
    // IPC
    int ipc_socket;
} wlt_compositor_t;

// A window (view)
typedef struct {
    struct wl_list link;                  // Link in compositor.views
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_scene_tree *scene_tree;
    bool mapped;
    bool minimized;
    bool maximized;
    bool fullscreen;
    struct wlr_box geometry;              // Position + size
    // Event listeners
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
    struct wl_listener request_move;
    struct wl_listener request_resize;
    struct wl_listener request_maximize;
    struct wl_listener request_fullscreen;
} wlt_view_t;

// An output (monitor)
typedef struct {
    struct wl_list link;                  // Link in compositor.outputs
    struct wlr_output *wlr_output;
    struct wlr_scene_output *scene_output;
    struct wl_listener frame;
    struct wl_listener destroy;
    struct wlr_box geometry;              // Position + resolution
} wlt_output_t;
```

## Event Loop

```
1. Initialize wlroots (backend, renderer, allocator)
2. Create scene graph (wlr_scene)
3. Detect outputs (monitors)
4. Set up input (keyboard, mouse, touch)
5. Set up protocol shells (xdg-shell, layer-shell)
6. Set up IPC socket (for WLT-Shell communication)
7. Enter main loop:
   a. wl_event_loop_dispatch() — process events
   b. On output frame: render scene → present to DRM
   c. On keyboard input: handle keybindings or send to focused client
   d. On mouse motion: move cursor, send to focused client
   e. On new surface: create view, add to scene
   f. On surface destroy: remove view, reassign focus
8. On shutdown: clean up all resources, exit
```

## Keybindings (Default)

| Key | Action |
|-----|--------|
| Super+D | Open launcher |
| Super+T | Open terminal |
| Super+E | Open file manager |
| Alt+Tab | Switch windows |
| Super+Left | Snap window left |
| Super+Right | Snap window right |
| Super+Up | Maximize window |
| Super+Down | Minimize window |
| Super+Shift+Q | Close window |
| Super+Shift+E | Exit compositor |
| Ctrl+Alt+Del | Open task manager |

## Config Format (`/etc/wlt/compositor.conf`)

```ini
[wlt-compositor]
backend = drm                # drm | headless | x11
renderer = gles2             # gles2 | vulkan | pixman
xwayland = yes               # Enable XWayland for X11 apps

[outputs]
mode = auto                  # auto | preferred | largest
scale = 1                    # HiDPI scale factor

[keybindings]
launcher = Super+d
terminal = Super+t
file_manager = Super+e
close = Super+Shift+q
exit = Super+Shift+e

[theme]
background = #1a1b26
border_color = #7aa2f7
border_width = 2
shadow = yes
corner_radius = 8
```

---

**Spec version:** 1.0
**Ready for implementation:** Yes (after seL4 + Linux boots with DRM support)
