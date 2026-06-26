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

    setenv("WLR_BACKENDS", "headless", 1);
    setenv("WLR_RENDERER", "pixman", 1);

    struct wl_display *display = wl_display_create();
    if (display == NULL)
        return WLT_COMP_ERR_BACKEND;
    state->wl_display = display;

    struct wl_event_loop *loop = wl_display_get_event_loop(display);
    struct wlr_backend *backend = wlr_headless_backend_create(loop);
    if (backend == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_BACKEND;
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

    struct wlr_output *output = wlr_headless_add_output(backend, 1366, 768);
    if (output == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_BACKEND;
    }
    wlr_output_set_name(output, "HEADLESS-1");
    wlr_output_layout_add_auto(output_layout, output);
    fprintf(stderr, "[compositor] Output HEADLESS-1 created (1366x768)\n");

    struct wlr_seat *seat = wlr_seat_create(display, "seat0");
    if (seat == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_seat = seat;

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