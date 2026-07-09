/**
 * @file compositor.c
 * @brief Main Wayland compositor — wlroots backend, event loop, scene graph
 * @module wlt-compositor
 * @purpose Initialize wlroots, manage outputs + views, run event loop
 * @dependencies: compositor.h, wlroots 0.18, wayland-server
 * @spec: docs/specs/wlt-compositor.md
 * @adr: docs/adr/05-wayland-wlroots.md
 *
 * v2: Headless backend + pixman + XDG shell (clients can create windows)
 */

#include "compositor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

/* Wayland server */
#include <wayland-server-core.h>

/* wlroots 0.18 */
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/render/pixman.h>

/* XDG shell listener */
static struct wl_listener xdg_surface_listener;

static void handle_xdg_surface(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_xdg_surface *xdg_surface = (struct wlr_xdg_surface *)data;
    if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    write(1, "[compositor] New window created\n", 32);
}

wlt_comp_error_t
wlt_comp_init(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    struct wl_display *display = wl_display_create();
    if (display == NULL)
        return WLT_COMP_ERR_BACKEND;
    state->wl_display = display;

    struct wl_event_loop *loop = wl_display_get_event_loop(display);

    /* Backend selection: try a real DRM display first, fall back to
     * headless. Every prior milestone (terminal, filemanager, settings)
     * has "connected to Wayland" per serial logs but has never actually
     * been seen — the compositor has only ever initialized headless. */
    struct wlr_backend *backend = NULL;
    bool using_drm = false;
    if (access("/dev/dri/card0", F_OK) == 0) {
        setenv("WLR_BACKENDS", "drm", 1);
        backend = wlr_backend_autocreate(loop, NULL);
        if (backend != NULL) {
            using_drm = true;
            fprintf(stderr, "[compositor] DRM backend created (/dev/dri/card0)\n");
        } else {
            fprintf(stderr, "[compositor] DRM backend creation failed, falling back to headless\n");
        }
    }
    if (backend == NULL) {
        setenv("WLR_BACKENDS", "headless", 1);
        setenv("WLR_RENDERER", "pixman", 1);
        backend = wlr_headless_backend_create(loop);
        if (backend == NULL) {
            wl_display_destroy(display);
            return WLT_COMP_ERR_BACKEND;
        }
    }
    state->wl_backend = backend;

    struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_RENDERER;
    }
    state->wl_renderer = renderer;

    struct wlr_allocator *allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_allocator = allocator;

    struct wlr_scene *scene = wlr_scene_create();
    wlr_compositor_create(display, 4, renderer);
    wlr_shm_create_with_renderer(display, 1, renderer);
    if (scene == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_scene = scene;

    struct wlr_output_layout *output_layout = wlr_output_layout_create(display);
    if (output_layout == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_output_layout = output_layout;

    if (using_drm) {
        /* DRM outputs are discovered by the backend via hotplug
         * (wlr_backend.events.new_output). A listener for that isn't wired
         * yet — this proves DRM backend creation itself works, which is
         * the gate everything else sits behind. If backend_start succeeds
         * here but no output ever appears, that's the next concrete gap,
         * not a reason this step failed. */
        fprintf(stderr, "[compositor] DRM backend active — outputs arrive via hotplug (listener not yet wired)\n");
    } else {
        struct wlr_output *output = wlr_headless_add_output(backend, 1366, 768);
        if (output == NULL) {
            wl_display_destroy(display);
            return WLT_COMP_ERR_BACKEND;
        }
        wlr_output_set_name(output, "HEADLESS-1");
        wlr_output_layout_add_auto(output_layout, output);
        fprintf(stderr, "[compositor] Output HEADLESS-1 created (1366x768)\n");
    }

    struct wlr_seat *seat = wlr_seat_create(display, "seat0");
    if (seat == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_seat = seat;
    /* Advertise keyboard capability so clients can get keyboard input */
    wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    /* XDG shell — allows clients to create real windows */
    struct wlr_xdg_shell *xdg_shell = wlr_xdg_shell_create(display, 3);
    if (xdg_shell) {
        xdg_surface_listener.notify = handle_xdg_surface;
        wl_signal_add(&xdg_shell->events.new_surface, &xdg_surface_listener);
        fprintf(stderr, "[compositor] XDG shell created\n");
    }

    const char *socket_name = wl_display_add_socket_auto(display);
    if (socket_name == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_IO;
    }
    fprintf(stderr, "[compositor] Wayland display running on %s\n", socket_name);

    state->running = false;
    state->focused_view_id = 0;
    return WLT_COMP_OK;
}

wlt_comp_error_t
wlt_comp_run(wlt_comp_state_t *state)
{
    if (state == NULL || state->wl_display == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    state->running = true;

    struct wlr_backend *backend = (struct wlr_backend *)state->wl_backend;
    if (!wlr_backend_start(backend)) {
        state->running = false;
        return WLT_COMP_ERR_BACKEND;
    }
    fprintf(stderr, "[compositor] Backend started, entering event loop\n");

    struct wl_display *display = (struct wl_display *)state->wl_display;
    wl_display_run(display);

    state->running = false;
    return WLT_COMP_OK;
}

void
wlt_comp_shutdown(wlt_comp_state_t *state)
{
    if (state == NULL) return;
    if (state->wl_display) {
        wl_display_destroy((struct wl_display *)state->wl_display);
    }
    memset(state, 0, sizeof(*state));
}

const char *
wlt_comp_error_str(wlt_comp_error_t err)
{
    switch (err) {
    case WLT_COMP_OK:               return "OK";
    case WLT_COMP_ERR_INVALID_ARG:  return "Invalid argument";
    case WLT_COMP_ERR_IO:           return "I/O error";
    case WLT_COMP_ERR_NOMEM:        return "Out of memory";
    case WLT_COMP_ERR_BACKEND:      return "Backend error";
    case WLT_COMP_ERR_RENDERER:     return "Renderer error";
    case WLT_COMP_ERR_NOT_FOUND:    return "Not found";
    default:                        return "Unknown error";
    }
}