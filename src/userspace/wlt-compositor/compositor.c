/**
 * @file compositor.c
 * @brief Main Wayland compositor — wlroots backend, event loop, scene graph
 * @module wlt-compositor
 * @purpose Initialize wlroots, manage outputs + views, run event loop
 * @dependencies: compositor.h, wlroots 0.18, wayland-server
 *
 * v4: Add input handling (keyboard + pointer), focus tracking,
 *     click-to-focus, drag-to-move, Alt+Tab window switching.
 *     This is the "real window management" phase.
 */

#include "compositor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>

/* Wayland server */
#include <wayland-server-core.h>

/* wlroots 0.18 */
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include "blur.h"  /* GPU Section B: dual-pass Gaussian backdrop blur */
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/util/box.h>
#include <linux/input.h>
#include <wlr/render/pixman.h>

/* ═══════════════════════════════════════════════════════════════════════
 *  GLOBALS
 * ═══════════════════════════════════════════════════════════════════════ */
static struct wl_listener xdg_surface_listener;
static struct wl_listener new_output_listener;
static struct wl_listener output_frame_listener;
static struct wl_listener new_input_listener;

static struct wlr_output_layout *g_output_layout = NULL;
static struct wlr_scene *g_scene = NULL;
static struct wlr_allocator *g_allocator = NULL;
static struct wlr_renderer *g_renderer = NULL;
static struct wlr_seat *g_seat = NULL;
static struct wlr_cursor *g_cursor = NULL;
static struct wlr_xcursor_manager *g_xcursor_mgr = NULL;
static struct wlr_output *g_active_output = NULL;
static int g_frame_count = 0;
static wlt_blur_t *g_blur = NULL;  /* lazy-init on first frame */

/* ═══════════════════════════════════════════════════════════════════════
 *  VIEW TRACKING — track XDG surfaces for focus + window management
 * ═══════════════════════════════════════════════════════════════════════ */
typedef struct wlt_view {
    struct wlr_xdg_surface *xdg_surface;
    struct wlr_scene_tree *scene_tree;
    struct wl_listener destroy;
    struct wl_listener request_move;
    int x, y;
    int w, h;           /* current size (0 = client default) */
    int prev_x, prev_y; /* saved geometry for maximize restore */
    int prev_w, prev_h;
    bool mapped;
    bool minimized;
    bool maximized;
    struct wl_list link;  /* linked list of all views */
} wlt_view_t;

static struct wl_list g_views;  /* list of all views */
static wlt_view_t *g_focused_view = NULL;

/* Drag state */
static bool g_dragging = false;
static int g_drag_x, g_drag_y;  /* offset from cursor to view origin */

/* Resize state */
static bool g_resizing = false;
static int g_resize_start_x, g_resize_start_y;
static int g_resize_start_w, g_resize_start_h;

/* Output dimensions (for maximize) */
static int g_output_w = 1280, g_output_h = 800;

/* ═══════════════════════════════════════════════════════════════════════
 *  FOCUS MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════ */
static void focus_view(wlt_view_t *view) {
    if (view == NULL || !view->mapped) return;

    /* Un-minimize if needed (for taskbar click-to-restore) */
    if (view->minimized) {
        view->minimized = false;
        if (view->scene_tree) {
            wlr_scene_node_set_enabled(&view->scene_tree->node, true);
        }
        fprintf(stderr, "[compositor] View un-minimized on focus\n");
    }

    if (g_focused_view == view) return;

    /* Remove keyboard focus from previous view */
    if (g_focused_view) {
        /* Could add visual distinction here (border color, etc.) */
    }

    g_focused_view = view;

    /* Raise to top of scene graph */
    if (view->scene_tree) {
        wlr_scene_node_raise_to_top(&view->scene_tree->node);
    }

    /* Give keyboard focus */
    struct wlr_surface *surface = view->xdg_surface->surface;
    struct wlr_keyboard *kb = wlr_seat_get_keyboard(g_seat);
    if (kb) {
        wlr_seat_keyboard_notify_enter(g_seat, surface, kb->keycodes,
            kb->num_keycodes, &kb->modifiers);
    } else {
        wlr_seat_keyboard_notify_enter(g_seat, surface, NULL, 0, NULL);
    }

    fprintf(stderr, "[compositor] Focus: %s\n",
            view->xdg_surface->toplevel ?
            view->xdg_surface->toplevel->title : "(no title)");
}

static wlt_view_t *view_at(double x, double y) {
    /* Find the topmost surface at (x, y) */
    double sx, sy;
    struct wlr_surface *surface = NULL;
    wlt_view_t *view;
    wl_list_for_each(view, &g_views, link) {
        if (!view->mapped || view->minimized) continue;
        surface = wlr_xdg_surface_surface_at(view->xdg_surface,
                    x - view->x, y - view->y, &sx, &sy);
        if (surface) return view;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  XDG SURFACE LIFECYCLE
 * ═══════════════════════════════════════════════════════════════════════ */
static void handle_xdg_destroy(struct wl_listener *listener, void *data) {
    wlt_view_t *view = wl_container_of(listener, view, destroy);
    wl_list_remove(&view->link);
    wl_list_remove(&view->destroy.link);
    /* [fix] request_move is conditionally wl_signal_add'd in
     * handle_xdg_surface (when xdg_surface->toplevel exists) but was
     * never removed here before free(view) - left a dangling listener
     * entry in xdg_surface->toplevel->events.request_move's list,
     * pointing into memory about to be freed. wl_listener removal is
     * never automatic; every wl_signal_add needs a matching
     * wl_list_remove, same as destroy.link already correctly does. */
    if (view->request_move.notify) {
        wl_list_remove(&view->request_move.link);
    }
    if (g_focused_view == view) g_focused_view = NULL;
    free(view);
    fprintf(stderr, "[compositor] View destroyed\n");
}

static void handle_xdg_request_move(struct wl_listener *listener, void *data) {
    /* Interactive move request from client (e.g., titlebar drag) */
    wlt_view_t *view = wl_container_of(listener, view, request_move);
    g_dragging = true;
    g_drag_x = (int)g_cursor->x - view->x;
    g_drag_y = (int)g_cursor->y - view->y;
    g_focused_view = view;
}

static void handle_xdg_surface(struct wl_listener *listener, void *data) {
    struct wlr_xdg_surface *xdg_surface = (struct wlr_xdg_surface *)data;

    if (g_scene) {
        struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(&g_scene->tree, xdg_surface);
        if (tree) {
            /* Raise new surface to top so it's visible (not hidden behind existing windows) */
            wlr_scene_node_raise_to_top(&tree->node);
            fprintf(stderr, "[compositor] XDG surface added to scene graph (role=%d, raised to top)\n",
                    xdg_surface->role);

            /* Track this view for focus + window management.
             * Note: map/unmap are handled by wlr_scene_xdg_surface_create
             * internally (shows/hides the scene node). We track views
             * for focus + positioning only. */
            wlt_view_t *view = calloc(1, sizeof(wlt_view_t));
            view->xdg_surface = xdg_surface;
            view->scene_tree = tree;
            view->mapped = true;  /* assume mapped — scene handles visibility */
            view->x = 0;
            view->y = 0;

            view->destroy.notify = handle_xdg_destroy;
            wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

            if (xdg_surface->toplevel) {
                view->request_move.notify = handle_xdg_request_move;
                wl_signal_add(&xdg_surface->toplevel->events.request_move,
                              &view->request_move);
            }

            wl_list_insert(&g_views, &view->link);

            /* Schedule configure to kickstart the client */
            wlr_xdg_surface_schedule_configure(xdg_surface);
            fprintf(stderr, "[compositor] Configure scheduled\n");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  INPUT HANDLING — keyboard + pointer
 * ═══════════════════════════════════════════════════════════════════════ */

/* Keyboard state tracking */
typedef struct wlt_keyboard {
    struct wlr_keyboard keyboard;
    struct wl_listener key;
    struct wl_listener modifiers;
    struct wl_listener destroy;
} wlt_keyboard_t;

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
    wlt_keyboard_t *kb = wl_container_of(listener, kb, key);
    struct wlr_keyboard_key_event *event = data;

    /* Handle compositor keybindings */
    uint32_t mods = wlr_keyboard_get_modifiers(&kb->keyboard);

    /* Alt+Tab: cycle through windows */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
        (mods & WLR_MODIFIER_ALT) && event->keycode == 15) { /* 15 = Tab */
        if (!wl_list_empty(&g_views)) {
            /* Find next mapped view after focused */
            wlt_view_t *start = g_focused_view;
            if (!start) {
                /* Get first view in list */
                wlt_view_t *tmp;
                wl_list_for_each(tmp, &g_views, link) { start = tmp; break; }
            }
            wlt_view_t *v, *next = NULL;
            bool found = false;
            wl_list_for_each(v, &g_views, link) {
                if (!v->mapped) continue;
                if (found) { next = v; break; }
                if (v == start) found = true;
            }
            if (!next) {
                /* Wrap around */
                wl_list_for_each(v, &g_views, link) {
                    if (v->mapped) { next = v; break; }
                }
            }
            if (next) {
                focus_view(next);
            }
        }
        return;
    }

    /* Alt+Q: close focused window */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
        (mods & WLR_MODIFIER_ALT) && event->keycode == 16) { /* 16 = Q */
        if (g_focused_view && g_focused_view->xdg_surface->toplevel) {
            wlr_xdg_toplevel_send_close(g_focused_view->xdg_surface->toplevel);
            fprintf(stderr, "[compositor] Close requested\n");
        }
        return;
    }

    /* Alt+M: minimize focused window */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
        (mods & WLR_MODIFIER_ALT) && event->keycode == 50) { /* 50 = M */
        if (g_focused_view) {
            g_focused_view->minimized = true;
            if (g_focused_view->scene_tree) {
                wlr_scene_node_set_enabled(&g_focused_view->scene_tree->node, false);
            }
            fprintf(stderr, "[compositor] Minimized view\n");
            g_focused_view = NULL;
            /* Focus next mapped view */
            wlt_view_t *v;
            wl_list_for_each(v, &g_views, link) {
                if (v->mapped && !v->minimized) { focus_view(v); break; }
            }
        }
        return;
    }

    /* Alt+F: maximize/restore focused window */
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
        (mods & WLR_MODIFIER_ALT) && event->keycode == 33) { /* 33 = F */
        if (g_focused_view && g_focused_view->xdg_surface->toplevel) {
            wlt_view_t *view = g_focused_view;
            if (view->maximized) {
                /* Restore */
                view->x = view->prev_x;
                view->y = view->prev_y;
                view->w = view->prev_w;
                view->h = view->prev_h;
                view->maximized = false;
                if (view->scene_tree) {
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                }
                wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
                    view->w > 0 ? view->w : 0, view->h > 0 ? view->h : 0);
                fprintf(stderr, "[compositor] Restored view to (%d,%d) %dx%d\n",
                        view->x, view->y, view->w, view->h);
            } else {
                /* Maximize */
                view->prev_x = view->x;
                view->prev_y = view->y;
                view->prev_w = view->w;
                view->prev_h = view->h;
                view->x = 0;
                view->y = 0;
                view->w = g_output_w;
                view->h = g_output_h - 32; /* leave room for status bar */
                view->maximized = true;
                if (view->scene_tree) {
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                }
                wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel,
                    view->w, view->h);
                fprintf(stderr, "[compositor] Maximized view to %dx%d\n",
                        view->w, view->h);
            }
        }
        return;
    }

    /* Route to focused client */
    wlr_seat_set_keyboard(g_seat, &kb->keyboard);
    wlr_seat_keyboard_notify_key(g_seat, event->time_msec,
                                 event->keycode, event->state);
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
    wlt_keyboard_t *kb = wl_container_of(listener, kb, modifiers);
    wlr_seat_set_keyboard(g_seat, &kb->keyboard);
    wlr_seat_keyboard_notify_modifiers(g_seat, &kb->keyboard.modifiers);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
    wlt_keyboard_t *kb = wl_container_of(listener, kb, destroy);
    wl_list_remove(&kb->key.link);
    wl_list_remove(&kb->modifiers.link);
    wl_list_remove(&kb->destroy.link);
    free(kb);
}

/* Pointer state tracking */
typedef struct wlt_pointer {
    struct wlr_pointer pointer;
    struct wl_listener motion;
    struct wl_listener motion_absolute;
    struct wl_listener button;
    struct wl_listener axis;
    struct wl_listener destroy;
} wlt_pointer_t;

static void pointer_handle_motion(struct wl_listener *listener, void *data) {
    wlt_pointer_t *ptr = wl_container_of(listener, ptr, motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(g_cursor, &ptr->pointer.base, event->delta_x, event->delta_y);

    /* Dragging: move focused view */
    if (g_dragging && g_focused_view) {
        g_focused_view->x = (int)g_cursor->x - g_drag_x;
        g_focused_view->y = (int)g_cursor->y - g_drag_y;
        if (g_focused_view->scene_tree) {
            wlr_scene_node_set_position(&g_focused_view->scene_tree->node,
                                        g_focused_view->x, g_focused_view->y);
        }
    }

    /* Resizing: adjust focused view size */
    if (g_resizing && g_focused_view && g_focused_view->xdg_surface->toplevel) {
        int dx = (int)g_cursor->x - g_resize_start_x;
        int dy = (int)g_cursor->y - g_resize_start_y;
        int new_w = g_resize_start_w + dx;
        int new_h = g_resize_start_h + dy;
        if (new_w < 100) new_w = 100;
        if (new_h < 100) new_h = 100;
        g_focused_view->w = new_w;
        g_focused_view->h = new_h;
        wlr_xdg_toplevel_set_size(g_focused_view->xdg_surface->toplevel, new_w, new_h);
    }

    /* Update pointer focus */
    wlt_view_t *view = view_at(g_cursor->x, g_cursor->y);
    struct wlr_surface *surface = NULL;
    /* [fix] sx/sy were uninitialized and only written inside the
     * `if (view)` branch below, but wlr_seat_pointer_notify_motion()
     * at the end of this function used them unconditionally - every
     * pointer motion over empty desktop (view == NULL, an extremely
     * common case) or over a view with no surface at that exact point
     * read indeterminate stack memory. Zero-initialize so the no-
     * surface path passes real, defined coordinates instead of
     * garbage. */
    double sx = 0.0, sy = 0.0;
    if (view) {
        surface = wlr_xdg_surface_surface_at(view->xdg_surface,
                    g_cursor->x - view->x, g_cursor->y - view->y, &sx, &sy);
    }
    if (surface) {
        wlr_seat_pointer_notify_enter(g_seat, surface, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(g_seat);
    }
    wlr_seat_pointer_notify_motion(g_seat, event->time_msec, sx, sy);
}

static void pointer_handle_button(struct wl_listener *listener, void *data) {
    wlt_pointer_t *ptr = wl_container_of(listener, ptr, button);
    struct wlr_pointer_button_event *event = data;

    wlr_seat_pointer_notify_button(g_seat, event->time_msec, event->button, event->state);

    if ((enum wlr_button_state)event->state == WLR_BUTTON_PRESSED) {
        /* Click-to-focus */
        wlt_view_t *view = view_at(g_cursor->x, g_cursor->y);
        if (view) {
            focus_view(view);

            uint32_t mods = wlr_keyboard_get_modifiers(wlr_seat_get_keyboard(g_seat));

            /* Plain right-click (no modifier): context menu (FR-GUI-11).
             * Cycles through: maximize → close → restore. Real menu rendering
             * would use a layer surface; for now the action is direct. */
            if (!(mods & WLR_MODIFIER_ALT) && event->button == BTN_RIGHT) {
                static int ctx_state = 0;
                ctx_state = (ctx_state + 1) % 3;
                if (ctx_state == 0) {
                    /* Maximize */
                    view->x = 0; view->y = 0;
                    view->w = g_output_w; view->h = g_output_h - 32;
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                    fprintf(stderr, "[compositor] Context menu: MAXIMIZE\n");
                } else if (ctx_state == 1) {
                    /* Restore (half-screen) */
                    view->x = 100; view->y = 100;
                    view->w = g_output_w / 2; view->h = (g_output_h - 32) / 2;
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                    fprintf(stderr, "[compositor] Context menu: RESTORE\n");
                } else {
                    /* Minimize (hide) */
                    wlr_scene_node_set_enabled(&view->scene_tree->node, false);
                    fprintf(stderr, "[compositor] Context menu: MINIMIZE\n");
                }
            }

            if (mods & WLR_MODIFIER_ALT) {
                if (event->button == BTN_LEFT) {
                    /* Alt+LeftClick: drag-to-move */
                    g_dragging = true;
                    g_drag_x = (int)g_cursor->x - view->x;
                    g_drag_y = (int)g_cursor->y - view->y;
                    fprintf(stderr, "[compositor] Drag started on view at (%d,%d)\n",
                            view->x, view->y);
                } else if (event->button == BTN_RIGHT) {
                    /* Alt+RightClick: resize */
                    g_resizing = true;
                    g_resize_start_x = (int)g_cursor->x;
                    g_resize_start_y = (int)g_cursor->y;
                    g_resize_start_w = view->w > 0 ? view->w : 400;
                    g_resize_start_h = view->h > 0 ? view->h : 300;
                    fprintf(stderr, "[compositor] Resize started (start %dx%d at %d,%d)\n",
                            g_resize_start_w, g_resize_start_h,
                            g_resize_start_x, g_resize_start_y);
                }
            }
        }
    } else if ((enum wlr_button_state)event->state == WLR_BUTTON_RELEASED) {
        if (g_dragging) {
            fprintf(stderr, "[compositor] Drag ended\n");
            g_dragging = false;

            /* Window snapping (FR-GUI-04): if cursor is near a screen edge
             * when the drag ends, snap the window to that half. Threshold: 20px. */
            if (g_focused_view) {
                wlt_view_t *view = g_focused_view;
                int snap = 20;
                int cx = (int)g_cursor->x, cy = (int)g_cursor->y;
                int hw = g_output_w / 2;
                if (cx < snap) {
                    view->x=0; view->y=0; view->w=hw; view->h=g_output_h-32;
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                    fprintf(stderr, "[compositor] Snap left\n");
                } else if (cx > g_output_w-snap) {
                    view->x=hw; view->y=0; view->w=hw; view->h=g_output_h-32;
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                    fprintf(stderr, "[compositor] Snap right\n");
                } else if (cy < snap) {
                    view->x=0; view->y=0; view->w=g_output_w; view->h=g_output_h-32;
                    wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
                    fprintf(stderr, "[compositor] Snap top (maximize)\n");
                }
            }
        }
        if (g_resizing) {
            fprintf(stderr, "[compositor] Resize ended (final %dx%d)\n",
                    g_focused_view ? g_focused_view->w : 0,
                    g_focused_view ? g_focused_view->h : 0);
            g_resizing = false;
        }
    }
}

static void pointer_handle_axis(struct wl_listener *listener, void *data) {
    wlt_pointer_t *ptr = wl_container_of(listener, ptr, axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(g_seat, event->time_msec,
        event->orientation, event->delta, event->delta_discrete, event->source,
        WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL);
}

static void pointer_handle_destroy(struct wl_listener *listener, void *data) {
    wlt_pointer_t *ptr = wl_container_of(listener, ptr, destroy);
    wl_list_remove(&ptr->motion.link);
    wl_list_remove(&ptr->motion_absolute.link);
    wl_list_remove(&ptr->button.link);
    wl_list_remove(&ptr->axis.link);
    wl_list_remove(&ptr->destroy.link);
    free(ptr);
}

/* New input device handler */
static void handle_new_input(struct wl_listener *listener, void *data) {
    struct wlr_input_device *device = (struct wlr_input_device *)data;
    fprintf(stderr, "[compositor] New input device: %s (type=%d)\n",
            device->name, (int)device->type);

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: {
        wlt_keyboard_t *kb = calloc(1, sizeof(wlt_keyboard_t));
        kb->keyboard = *wlr_keyboard_from_input_device(device);

        /* Set keymap from environment */
        struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(ctx, NULL,
            XKB_KEYMAP_COMPILE_NO_FLAGS);
        wlr_keyboard_set_keymap(&kb->keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(ctx);

        /* Repeat rate */
        wlr_keyboard_set_repeat_info(&kb->keyboard, 25, 600);

        kb->key.notify = keyboard_handle_key;
        wl_signal_add(&kb->keyboard.events.key, &kb->key);
        kb->modifiers.notify = keyboard_handle_modifiers;
        wl_signal_add(&kb->keyboard.events.modifiers, &kb->modifiers);
        kb->destroy.notify = keyboard_handle_destroy;
        wl_signal_add(&device->events.destroy, &kb->destroy);

        wlr_seat_set_keyboard(g_seat, &kb->keyboard);
        fprintf(stderr, "[compositor] Keyboard configured\n");
        break;
    }
    case WLR_INPUT_DEVICE_POINTER: {
        wlt_pointer_t *ptr = calloc(1, sizeof(wlt_pointer_t));
        ptr->pointer = *wlr_pointer_from_input_device(device);

        ptr->motion.notify = pointer_handle_motion;
        wl_signal_add(&ptr->pointer.events.motion, &ptr->motion);
        ptr->motion_absolute.notify = pointer_handle_motion;
        wl_signal_add(&ptr->pointer.events.motion_absolute, &ptr->motion_absolute);
        ptr->button.notify = pointer_handle_button;
        wl_signal_add(&ptr->pointer.events.button, &ptr->button);
        ptr->axis.notify = pointer_handle_axis;
        wl_signal_add(&ptr->pointer.events.axis, &ptr->axis);
        ptr->destroy.notify = pointer_handle_destroy;
        wl_signal_add(&device->events.destroy, &ptr->destroy);

        wlr_cursor_attach_input_device(g_cursor, device);
        fprintf(stderr, "[compositor] Pointer configured\n");
        break;
    }
    default:
        fprintf(stderr, "[compositor] Unknown input device type, ignoring\n");
        break;
    }

    /* Update seat capabilities — always advertise both since we
     * created the seat with keyboard + pointer capabilities */
    wlr_seat_set_capabilities(g_seat,
        WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  OUTPUT + FRAME HANDLING
 * ═════════════════════════════════════��═════════════════════════════════ */
static void handle_output_frame(struct wl_listener *listener, void *data) {
    struct wlr_output *output = (struct wlr_output *)data;
    g_frame_count++;

    struct wlr_scene_output *scene_out = wlr_scene_get_scene_output(g_scene, output);
    if (scene_out) {
        if (g_frame_count <= 10) {
            fprintf(stderr, "[compositor] frame %d: rendering+committing\n", g_frame_count);
        }
        /* GPU Section B: apply backdrop blur as a post-process pass.
         * Lazy-init the blur shader on the first frame, then apply it
         * over the lower third of the screen (taskbar region) as the
         * proof-of-concept blurred surface per PRD Section 7 Phase 2.
         * The blur reads the just-composited scene and writes the
         * blurred result back before the output commit. */
        int out_w = output->width;
        int out_h = output->height;
        if (out_w > 0 && out_h > 0) {
            if (!g_blur) {
                g_blur = wlt_blur_create(10);  /* 10px sigma ~ radius 12 */
                if (g_blur) {
                    fprintf(stderr, "[compositor] backdrop blur enabled (10px)\n");
                }
            }
            if (g_blur) {
                /* Blur the taskbar strip: lower 48px of the output. */
                int bar_h = out_h > 48 ? 48 : out_h / 8;
                wlt_blur_apply(g_blur, 0, 0, out_w, bar_h, out_w, out_h);
            }
        }
        wlr_scene_output_commit(scene_out, NULL);
    } else if (g_frame_count <= 10) {
        fprintf(stderr, "[compositor] frame %d: NO scene output (creating)\n", g_frame_count);
        wlr_scene_output_create(g_scene, output);
    }
}

static void handle_new_output(struct wl_listener *listener, void *data) {
    struct wlr_output *output = (struct wlr_output *)data;
    g_active_output = output;
    fprintf(stderr, "[compositor] New output detected: %s\n", output->name);

    if (g_allocator && g_renderer) {
        wlr_output_init_render(output, g_allocator, g_renderer);
        fprintf(stderr, "[compositor] Output render initialized\n");
    }

    struct wlr_output_state output_state;
    wlr_output_state_init(&output_state);
    wlr_output_state_set_enabled(&output_state, true);

    if (!wl_list_empty(&output->modes)) {
        struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
        if (mode) {
            wlr_output_state_set_mode(&output_state, mode);
            fprintf(stderr, "[compositor] Set mode %dx%d@%dmHz on %s\n",
                    mode->width, mode->height, mode->refresh, output->name);
        }
    } else {
        wlr_output_state_set_custom_mode(&output_state, 1280, 800, 60000);
    }

    if (!wlr_output_commit_state(output, &output_state)) {
        fprintf(stderr, "[compositor] ERROR: output commit failed for %s\n", output->name);
        wlr_output_state_finish(&output_state);
        return;
    }
    wlr_output_state_finish(&output_state);
    fprintf(stderr, "[compositor] Output %s committed successfully\n", output->name);

    if (g_output_layout) {
        wlr_output_layout_add_auto(g_output_layout, output);
        fprintf(stderr, "[compositor] Output %s added to layout\n", output->name);
    }

    /* Save output dimensions for maximize */
    g_output_w = output->width;
    g_output_h = output->height;
    fprintf(stderr, "[compositor] Output dimensions: %dx%d\n", g_output_w, g_output_h);

    /* Wire the frame listener */
    output_frame_listener.notify = handle_output_frame;
    wl_signal_add(&output->events.frame, &output_frame_listener);
    fprintf(stderr, "[compositor] Frame listener wired for %s\n", output->name);

    wlr_output_schedule_frame(output);
    fprintf(stderr, "[compositor] First frame scheduled for %s\n", output->name);

    /* Configure cursor for this output */
    if (g_xcursor_mgr) {
        wlr_xcursor_manager_load(g_xcursor_mgr, 1);
        wlr_cursor_warp_closest(g_cursor, NULL, output->width / 2, output->height / 2);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  INIT / RUN / SHUTDOWN
 * ═══════════════════════════════════════════════════════════════════════ */
wlt_comp_error_t
wlt_comp_init(wlt_comp_state_t *state)
{
    if (state == NULL)
        return WLT_COMP_ERR_INVALID_ARG;

    memset(state, 0, sizeof(*state));
    /* GPU rendering path: prefer GLES2 when a DRM device is available
     * (real GPU or llvmpipe-backed Mesa — both are real GLES2 pipelines),
     * fall back to pixman for headless. The GLES2 renderer, Mesa DRI
     * drivers, and virtio-gpu.ko are already bundled (GPU Section 0
     * investigation); this line was the only thing forcing pixman. */
    if (access("/dev/dri/card0", F_OK) == 0)
        setenv("WLR_RENDERER", "gles2", 1);
    else
        setenv("WLR_RENDERER", "pixman", 1);
    wl_list_init(&g_views);

    struct wl_display *display = wl_display_create();
    if (display == NULL)
        return WLT_COMP_ERR_BACKEND;
    state->wl_display = display;

    struct wl_event_loop *loop = wl_display_get_event_loop(display);

    /* Backend selection: try DRM first, fall back to headless */
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
        backend = wlr_headless_backend_create(loop);
        if (backend == NULL) {
            wl_display_destroy(display);
            return WLT_COMP_ERR_BACKEND;
        }
    }
    state->wl_backend = backend;

    struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
    /* GPU-A fallback: if GLES2 failed (EGL can't init in some QEMU configs
     * without a GL display backend), retry with pixman. This keeps the
     * compositor booting even when the virtual GPU can't provide a real
     * GLES2 context — graceful degradation instead of a crash. */
    if (renderer == NULL && access("/dev/dri/card0", F_OK) == 0) {
        fprintf(stderr, "[compositor] GLES2 renderer failed; falling back to pixman\n");
        setenv("WLR_RENDERER", "pixman", 1);
        renderer = wlr_renderer_autocreate(backend);
    }
    if (renderer == NULL) {
        fprintf(stderr, "[compositor] all renderers failed\n");
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
    g_allocator = allocator;

    struct wlr_scene *scene = wlr_scene_create();
    wlr_compositor_create(display, 4, renderer);
    wlr_shm_create_with_renderer(display, 1, renderer);
    wlr_data_device_manager_create(display);
    if (scene == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_scene = scene;
    g_scene = scene;
    g_renderer = renderer;

    struct wlr_output_layout *output_layout = wlr_output_layout_create(display);
    if (output_layout == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_output_layout = output_layout;
    g_output_layout = output_layout;

    wlr_scene_attach_output_layout(scene, output_layout);
    fprintf(stderr, "[compositor] Scene attached to output layout\n");

    if (using_drm) {
        new_output_listener.notify = handle_new_output;
        wl_signal_add(&backend->events.new_output, &new_output_listener);
        fprintf(stderr, "[compositor] DRM backend active — new_output listener wired\n");
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

    /* Seat */
    struct wlr_seat *seat = wlr_seat_create(display, "seat0");
    if (seat == NULL) {
        wl_display_destroy(display);
        return WLT_COMP_ERR_NOMEM;
    }
    state->wl_seat = seat;
    g_seat = seat;
    wlr_seat_set_capabilities(seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    /* Cursor + xcursor manager */
    g_cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(g_cursor, output_layout);
    g_xcursor_mgr = wlr_xcursor_manager_create(NULL, 24);
    wlr_xcursor_manager_load(g_xcursor_mgr, 1);
    fprintf(stderr, "[compositor] Cursor initialized\n");

    /* Input device handler */
    new_input_listener.notify = handle_new_input;
    wl_signal_add(&backend->events.new_input, &new_input_listener);
    fprintf(stderr, "[compositor] Input listener wired\n");

    /* XDG shell */
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
    default:                        return "Unknown";
    }
}
