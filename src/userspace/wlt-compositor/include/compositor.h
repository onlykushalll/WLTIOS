/**
 * @file compositor.h
 * @brief Public API for WLT-Compositor (Wayland compositor)
 * @module wlt-compositor
 * @purpose Manage displays, inputs, windows via wlroots
 * @dependencies: wlroots, wayland
 * @tested_by: tests/userspace/compositor_test.c
 * @spec: docs/specs/wlt-compositor.md
 * @adr: docs/adr/05-wayland-wlroots.md
 */

#ifndef WLT_COMPOSITOR_H
#define WLT_COMPOSITOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_COMP_OK = 0,
    WLT_COMP_ERR_INVALID_ARG = -1,
    WLT_COMP_ERR_IO = -2,
    WLT_COMP_ERR_NOMEM = -3,
    WLT_COMP_ERR_BACKEND = -4,
    WLT_COMP_ERR_RENDERER = -5,
    WLT_COMP_ERR_NOT_FOUND = -6,
} wlt_comp_error_t;

#define WLT_COMP_VERSION "2.0.0"
#define WLT_COMP_MAX_OUTPUTS 4
#define WLT_COMP_MAX_VIEWS 64
#define WLT_COMP_MAX_NAME 128

typedef enum {
    WLT_COMP_VIEW_MAPPED = 0,
    WLT_COMP_VIEW_UNMAPPED,
    WLT_COMP_VIEW_FULLSCREEN,
    WLT_COMP_VIEW_MAXIMIZED,
    WLT_COMP_VIEW_MINIMIZED,
} wlt_comp_view_state_t;

typedef struct {
    uint32_t id;
    char title[WLT_COMP_MAX_NAME];
    int32_t x, y;
    int32_t width, height;
    wlt_comp_view_state_t state;
    bool focused;
    uint32_t app_id;
} wlt_comp_view_t;

typedef struct {
    char name[WLT_COMP_MAX_NAME];
    int32_t width;
    int32_t height;
    int32_t refresh_rate;
    bool enabled;
    bool primary;
} wlt_comp_output_t;

typedef struct {
    wlt_comp_output_t outputs[WLT_COMP_MAX_OUTPUTS];
    size_t output_count;
    wlt_comp_view_t views[WLT_COMP_MAX_VIEWS];
    size_t view_count;
    uint32_t focused_view_id;
    bool running;

    /* wlroots opaque pointers (cast to real types in compositor.c) */
    void *wl_display;       /* struct wl_display* */
    void *wl_backend;       /* struct wlr_backend* */
    void *wl_renderer;      /* struct wlr_renderer* */
    void *wl_allocator;     /* struct wlr_allocator* */
    void *wl_scene;         /* struct wlr_scene* */
    void *wl_output_layout; /* struct wlr_output_layout* */
    void *wl_seat;          /* struct wlr_seat* */
} wlt_comp_state_t;

wlt_comp_error_t wlt_comp_init(wlt_comp_state_t *state);
wlt_comp_error_t wlt_comp_run(wlt_comp_state_t *state);
void wlt_comp_shutdown(wlt_comp_state_t *state);
const char *wlt_comp_error_str(wlt_comp_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* WLT_COMPOSITOR_H */
