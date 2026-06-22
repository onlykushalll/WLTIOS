/**
 * @file widgets.h
 * @brief Public API for WLT-Widgets (UI toolkit)
 * @module wlt-widgets
 * @purpose Widget tree, buttons, containers, text, events, themes
 * @dependencies: render.h
 * @tested_by: tests/userspace/widgets_test.c
 * @spec: docs/specs/wlt-compositor.md
 */

#ifndef WLT_WIDGETS_H
#define WLT_WIDGETS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_W_OK = 0,
    WLT_W_ERR_INVALID_ARG = -1,
    WLT_W_ERR_NOMEM = -2,
    WLT_W_ERR_NOT_FOUND = -3,
} wlt_widget_error_t;

#define WLT_W_MAX_CHILDREN 32
#define WLT_W_MAX_TEXT 256

typedef enum {
    WLT_W_TYPE_ROOT = 0,
    WLT_W_TYPE_BUTTON,
    WLT_W_TYPE_LABEL,
    WLT_W_TYPE_TEXT,
    WLT_W_TYPE_CONTAINER,
    WLT_W_TYPE_IMAGE,
} wlt_widget_type_t;

typedef enum {
    WLT_W_LAYOUT_NONE = 0,
    WLT_W_LAYOUT_VBOX,
    WLT_W_LAYOUT_HBOX,
    WLT_W_LAYOUT_GRID,
} wlt_widget_layout_t;

typedef struct wlt_widget {
    uint32_t id;
    wlt_widget_type_t type;
    int32_t x, y, width, height;
    bool visible;
    bool focused;
    char text[WLT_W_MAX_TEXT];
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t border_color;
    int32_t border_radius;
    int32_t padding;
    wlt_widget_layout_t layout;
    struct wlt_widget *children[WLT_W_MAX_CHILDREN];
    size_t child_count;
    struct wlt_widget *parent;
    void (*on_click)(struct wlt_widget *self);
    void (*on_focus)(struct wlt_widget *self);
} wlt_widget_t;

wlt_widget_t *wlt_widget_create(wlt_widget_type_t type);
void wlt_widget_destroy(wlt_widget_t *widget);
wlt_widget_error_t wlt_widget_add_child(wlt_widget_t *parent, wlt_widget_t *child);
wlt_widget_error_t wlt_widget_remove_child(wlt_widget_t *parent, wlt_widget_t *child);
wlt_widget_error_t wlt_widget_set_text(wlt_widget_t *widget, const char *text);
wlt_widget_error_t wlt_widget_set_position(wlt_widget_t *widget, int32_t x, int32_t y);
wlt_widget_error_t wlt_widget_set_size(wlt_widget_t *widget, int32_t w, int32_t h);
wlt_widget_error_t wlt_widget_set_color(wlt_widget_t *widget, uint32_t bg, uint32_t fg);
wlt_widget_error_t wlt_widget_layout(wlt_widget_t *root);
wlt_widget_error_t wlt_widget_draw(wlt_widget_t *root);
wlt_widget_error_t wlt_widget_dispatch_event(wlt_widget_t *root, int32_t x, int32_t y, bool click);

#ifdef __cplusplus
}
#endif

#endif /* WLT_WIDGETS_H */
