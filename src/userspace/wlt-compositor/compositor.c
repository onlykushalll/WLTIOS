/**
 * @file compositor.c
 * @brief Main Wayland compositor — wlroots backend, event loop, scene graph
 * @module wlt-compositor
 * @purpose Initialize wlroots, manage outputs + views, run event loop
 * @dependencies: compositor.h
 * @tested_by: tests/userspace/compositor_test.c
 * @spec: docs/specs/wlt-compositor.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#include "compositor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* TODO(wlt-compositor): When wlroots is available, include:
 * #include <wlr/backend.h>
 * #include <wlr/render/wlr_renderer.h>
 * #include <wlr/types/wlr_scene.h>
 * #include <wlr/types/wlr_output_layout.h>
 * #include <wlr/types/wlr_seat.h>
 * #include <wlr/types/wlr_xdg_shell.h>
 * #include <wlr/types/wlr_layer_shell_v1.h>
 */

wlt_comp_error_t
wlt_comp_init(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));
    state->running = false;
    state->focused_view_id = 0;

    /* TODO(wlt-compositor): Real wlroots initialization:
     *   1. wl_display_create()
     *   2. wlr_backend_autocreate(display)
     *   3. wlr_renderer_autocreate(backend)
     *   4. wlr_allocator_autocreate(backend, renderer)
     *   5. wlr_scene_create()
     *   6. wlr_output_layout_create()
     *   7. wlr_seat_create(display, "seat0")
     *   8. wlr_xdg_shell_create(display, 2)
     *   9. wlr_layer_shell_v1_create(display, 3)
     *  10. Register signal handlers for new_output, new_xdg_surface, etc.
     */

    return WLT_COMP_OK;
}

wlt_comp_error_t
wlt_comp_run(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    state->running = true;

    /* TODO(wlt-compositor): Real event loop:
     *   wl_display_run(display)
     *
     * For now, a simple poll loop
     */
    while (state->running)
    {
        /* Process Wayland events */
        /* wl_display_flush_clients(display); */
        /* wl_event_loop_dispatch(loop, -1); */
        usleep(16000);  /* ~60fps */
    }

    return WLT_COMP_OK;
}

void
wlt_comp_shutdown(wlt_comp_state_t *state)
{
    if (state == NULL)
        return;

    state->running = false;

    /* TODO(wlt-compositor): Clean up wlroots resources:
     *   wlr_backend_destroy(backend)
     *   wl_display_destroy(display)
     */

    memset(state, 0, sizeof(*state));
}

wlt_comp_error_t
wlt_comp_view_create(wlt_comp_state_t *state, const char *title,
                     uint32_t app_id, uint32_t *view_id)
{
    if (state == NULL || title == NULL || view_id == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    if (state->view_count >= WLT_COMP_MAX_VIEWS)
        return WLT_COMP_ERR_NOMEM;

    wlt_comp_view_t *view = &state->views[state->view_count];
    memset(view, 0, sizeof(*view));
    view->id = (uint32_t)(state->view_count + 1);
    strncpy(view->title, title, WLT_COMP_MAX_NAME - 1);
    view->app_id = app_id;
    view->state = WLT_COMP_VIEW_MAPPED;
    view->x = 100;
    view->y = 100;
    view->width = 800;
    view->height = 600;
    view->focused = false;

    *view_id = view->id;
    state->view_count++;

    /* Auto-focus new view */
    wlt_comp_view_focus(state, view->id);

    return WLT_COMP_OK;
}

wlt_comp_error_t
wlt_comp_view_close(wlt_comp_state_t *state, uint32_t view_id)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            /* Shift remaining views */
            for (size_t j = i + 1; j < state->view_count; j++)
                state->views[j - 1] = state->views[j];
            state->view_count--;

            /* Focus next view */
            if (state->focused_view_id == view_id && state->view_count > 0)
                wlt_comp_view_focus(state, state->views[0].id);

            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_focus(wlt_comp_state_t *state, uint32_t view_id)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    /* Unfocus current */
    for (size_t i = 0; i < state->view_count; i++)
        state->views[i].focused = false;

    /* Focus requested */
    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            state->views[i].focused = true;
            state->focused_view_id = view_id;

            /* Move to end of array (top of z-order) */
            wlt_comp_view_t tmp = state->views[i];
            for (size_t j = i + 1; j < state->view_count; j++)
                state->views[j - 1] = state->views[j];
            state->views[state->view_count - 1] = tmp;

            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_move(wlt_comp_state_t *state, uint32_t view_id,
                   int32_t x, int32_t y)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            state->views[i].x = x;
            state->views[i].y = y;
            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_resize(wlt_comp_state_t *state, uint32_t view_id,
                     int32_t width, int32_t height)
{
    if (state == NULL || width < 1 || height < 1)
        return WLT_COMP_ERR_INVALID_ARG;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            state->views[i].width = width;
            state->views[i].height = height;
            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_maximize(wlt_comp_state_t *state, uint32_t view_id)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            if (state->views[i].state == WLT_COMP_VIEW_MAXIMIZED)
            {
                state->views[i].state = WLT_COMP_VIEW_MAPPED;
            }
            else
            {
                state->views[i].state = WLT_COMP_VIEW_MAXIMIZED;
                /* Use primary output dimensions */
                if (state->output_count > 0)
                {
                    state->views[i].x = 0;
                    state->views[i].y = 0;
                    state->views[i].width = state->outputs[0].width;
                    state->views[i].height = state->outputs[0].height;
                }
            }
            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_fullscreen(wlt_comp_state_t *state, uint32_t view_id)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            if (state->views[i].state == WLT_COMP_VIEW_FULLSCREEN)
                state->views[i].state = WLT_COMP_VIEW_MAPPED;
            else
                state->views[i].state = WLT_COMP_VIEW_FULLSCREEN;
            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_snap(wlt_comp_state_t *state, uint32_t view_id, int direction)
{
    if (state == NULL || state->output_count == 0)
        return WLT_COMP_ERR_INVALID_ARG;

    int32_t sw = state->outputs[0].width;
    int32_t sh = state->outputs[0].height;

    for (size_t i = 0; i < state->view_count; i++)
    {
        if (state->views[i].id == view_id)
        {
            switch (direction)
            {
                case 0: /* left half */
                    state->views[i].x = 0;
                    state->views[i].y = 0;
                    state->views[i].width = sw / 2;
                    state->views[i].height = sh;
                    break;
                case 1: /* right half */
                    state->views[i].x = sw / 2;
                    state->views[i].y = 0;
                    state->views[i].width = sw / 2;
                    state->views[i].height = sh;
                    break;
                case 2: /* maximize */
                    return wlt_comp_view_maximize(state, view_id);
                case 3: /* minimize */
                    state->views[i].state = WLT_COMP_VIEW_MINIMIZED;
                    break;
            }
            return WLT_COMP_OK;
        }
    }

    return WLT_COMP_ERR_NOT_FOUND;
}

wlt_comp_error_t
wlt_comp_view_cycle(wlt_comp_state_t *state)
{
    if (state == NULL || state->view_count < 2)
        return WLT_COMP_OK;

    /* Cycle focus to next view */
    uint32_t next = state->views[0].id;
    return wlt_comp_view_focus(state, next);
}

wlt_comp_error_t
wlt_comp_handle_key(wlt_comp_state_t *state, uint32_t key, uint32_t mods, bool pressed)
{
    if (state == NULL || !pressed)
        return WLT_COMP_OK;

    /* Super+D = launcher (handled by shell) */
    /* Super+T = terminal (handled by shell) */
    /* Alt+Tab = cycle views */
    if (mods == 0x08 && key == 15)  /* Alt+Tab */
    {
        return wlt_comp_view_cycle(state);
    }

    /* Super+Left/Right/Up/Down = snap */
    if (mods == 0x40)  /* Super */
    {
        switch (key)
        {
            case 113: return wlt_comp_view_snap(state, state->focused_view_id, 0); /* Left */
            case 114: return wlt_comp_view_snap(state, state->focused_view_id, 1); /* Right */
            case 116: return wlt_comp_view_maximize(state, state->focused_view_id);  /* Up */
            case 111: return wlt_comp_view_fullscreen(state, state->focused_view_id); /* F11 */
        }
    }

    return WLT_COMP_OK;
}

wlt_comp_error_t
wlt_comp_output_detect(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    /* TODO(wlt-compositor): Real output detection via wlroots:
     *   Listen for backend.events.new_output
     *   For each output: wlr_output_set_mode, wlr_output_enable, wlr_output_commit
     */

    /* For now, add a default 1366x768 output */
    if (state->output_count == 0)
    {
        wlt_comp_output_t *out = &state->outputs[0];
        memset(out, 0, sizeof(*out));
        strncpy(out->name, "DISPLAY1", WLT_COMP_MAX_NAME - 1);
        out->width = 1366;
        out->height = 768;
        out->refresh_rate = 60;
        out->enabled = true;
        out->primary = true;
        state->output_count = 1;
    }

    return WLT_COMP_OK;
}

wlt_comp_error_t
wlt_comp_output_set_mode(wlt_comp_state_t *state, size_t index,
                         int32_t width, int32_t height, int32_t refresh)
{
    if (state == NULL || index >= state->output_count)
        return WLT_COMP_ERR_INVALID_ARG;

    state->outputs[index].width = width;
    state->outputs[index].height = height;
    state->outputs[index].refresh_rate = refresh;

    return WLT_COMP_OK;
}

const char *
wlt_comp_error_str(wlt_comp_error_t err)
{
    switch (err)
    {
        case WLT_COMP_OK:              return "OK";
        case WLT_COMP_ERR_INVALID_ARG: return "Invalid argument";
        case WLT_COMP_ERR_IO:          return "I/O error";
        case WLT_COMP_ERR_NOMEM:       return "Out of memory";
        case WLT_COMP_ERR_BACKEND:     return "Backend error";
        case WLT_COMP_ERR_RENDERER:    return "Renderer error";
        case WLT_COMP_ERR_NOT_FOUND:   return "Not found";
        default:                       return "Unknown error";
    }
}
