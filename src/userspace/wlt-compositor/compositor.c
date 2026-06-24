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
 * Updated 2026-06-24: Headless backend + pixman renderer for WSL2/QEMU testing.
 *   - WLR_BACKENDS=headless forces the headless backend (no DRM needed)
 *   - WLR_RENDERER=pixman forces the software renderer (no GPU needed)
 *   - A virtual 1366x768 output is created so there's something to render to
 *   - The compositor runs in the Linux guest (seL4 PCI passthrough is a
 *     separate task; for now Linux sees the framebuffer directly)
 *
 * Honest seL4 gap: The compositor runs in the Linux guest, not in a seL4
 * partition. The security architecture (compositor isolation) is not yet
 * active. This is documented in docs/context/INTEGRATION_TEST_RESULTS.md.
 */

#include "compositor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
#include <wlr/render/pixman.h>

wlt_comp_error_t
wlt_comp_init(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));

    /* 0. Force headless backend + pixman renderer (no DRM/GPU needed) */
    setenv("WLR_BACKENDS", "headless", 1);
    setenv("WLR_RENDERER", "pixman", 1);

    /* 1. Create Wayland display */
    struct wl_display *display = wl_display_create();
    if (display == NULL)
        return WLT_COMP_ERR_BACKEND;
    state->wl_display = display;

    /* 2. Create headless backend explicitly (avoids DRM autodetect failure) */
    struct wl_event_loop *loop = wl_display_get_event_loop(display);
    struct wlr_backend *backend = wlr_headless_backend_create(loop);
    if (backend == NULL) {
        fprintf(stderr, "[compositor] Failed to create headless backend\n");
        wl_display_destroy(display);
        state->wl_display = NULL;
        return WLT_COMP_ERR_BACKEND;
    }
    state->wl_backend = backend;

    /* 3. Create renderer (pixman software renderer — no GPU needed) */
    struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
    if (renderer == NULL) {
        fprintf(stderr, "[compositor] Failed to create renderer\n");
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_RENDERER;
    }
    state->wl_renderer = renderer;

    /* 4. Create allocator */
    struct wlr_allocator *allocator = wlr_allocator_autocreate(backend, renderer);
    if (allocator == NULL) {
        fprintf(stderr, "[compositor] Failed to create allocator\n");
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

    /* 7. Create a virtual headless output (1366x768 — matches Kushal's display) */
    struct wlr_output *output = wlr_headless_add_output(backend, 1366, 768);
    if (output == NULL) {
        fprintf(stderr, "[compositor] Failed to create headless output\n");
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_BACKEND;
    }
    wlr_output_set_name(output, "HEADLESS-1");
    wlr_output_layout_add_auto(output_layout, output);
    fprintf(stderr, "[compositor] Output HEADLESS-1 created (1366x768)\n");

    /* 8. Create seat */
    struct wlr_seat *seat = wlr_seat_create(display, "seat0");
    if (seat == NULL) {
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_seat = seat;

    /* 9. Add Wayland socket (so clients can connect) */
    const char *socket_name = wl_display_add_socket_auto(display);
    if (socket_name == NULL) {
        fprintf(stderr, "[compositor] Failed to create Wayland socket\n");
        wl_display_destroy(display);
        memset(state, 0, sizeof(*state));
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

    /* Start the backend */
    struct wlr_backend *backend = (struct wlr_backend *)state->wl_backend;
    if (!wlr_backend_start(backend)) {
        fprintf(stderr, "[compositor] Failed to start backend\n");
        state->running = false;
        return WLT_COMP_ERR_BACKEND;
    }
    fprintf(stderr, "[compositor] Backend started, entering event loop\n");

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