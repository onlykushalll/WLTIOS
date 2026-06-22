/**
 * @file compositor.c
 * @brief Main Wayland compositor — wlroots backend, event loop, scene graph
 * @module wlt-compositor
 * @purpose Initialize wlroots, manage outputs + views, run event loop
 * @dependencies: compositor.h, wlroots 0.18, wayland-server
 * @tested_by: tests/userspace/compositor_test.c
 * @spec: docs/specs/wlt-compositor.md
 * @adr: docs/adr/05-wayland-wlroots.md
 *
 * Updated 2026-06-22: Replaced stub init with real wlroots 0.18 initialization.
 * Compiles and links against wlroots 0.18.2 (installed in WSL2).
 */

#include "compositor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Wayland server */
#include <wayland-server-core.h>

/* wlroots 0.18 */
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>

wlt_comp_error_t
wlt_comp_init(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    /* 1. Create Wayland display */
    struct wl_display *display = wl_display_create();
    if (display == NULL)
        return WLT_COMP_ERR_BACKEND;
    state->wl_display = display;

    /* 2. Create backend (auto-detect: DRM/KMS, Wayland, X11, or headless) */
    struct wl_event_loop *loop = wl_display_get_event_loop(display);
    struct wlr_backend *backend = wlr_backend_autocreate(loop, NULL);
    if (backend == NULL) {
        wl_display_destroy(display);
        state->wl_display = NULL;
        return WLT_COMP_ERR_BACKEND;
    }
    state->wl_backend = backend;

    /* 3. Create renderer */
    struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_RENDERER;
    }
    state->wl_renderer = renderer;

    /* 4. Create allocator */
    struct wlr_allocator *allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_allocator = allocator;

    /* 5. Create scene graph */
    struct wlr_scene *scene = wlr_scene_create();
    if (scene == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_scene = scene;

    /* 6. Create output layout */
    struct wlr_output_layout *output_layout = wlr_output_layout_create(display);
    if (output_layout == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_output_layout = output_layout;

    /* 7. Create seat */
    struct wlr_seat *seat = wlr_seat_create(display, "seat0");
    if (seat == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_seat = seat;

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

    /* Start the backend */
    struct wlr_backend *backend = (struct wlr_backend *)state->wl_backend;
    if (!wlr_backend_start(backend)) {
        state->running = false;
        return WLT_COMP_ERR_BACKEND;
    }

    /* Run the Wayland display event loop (blocks until wl_display_terminate) */
    struct wl_display *display = (struct wl_display *)state->wl_display;
    wl_display_run(display);

    state->running = false;
    return WLT_COMP_OK;
}

void
wlt_comp_shutdown(wlt_comp_state_t *state)
{
    if (state == NULL)
        return;

    if (state->wl_display) {
        struct wl_display *display = (struct wl_display *)state->wl_display;
        wl_display_destroy(display);
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
