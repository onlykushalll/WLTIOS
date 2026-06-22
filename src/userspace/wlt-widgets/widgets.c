/**
 * @file widgets.c
 * @brief Widget tree — create, destroy, layout, draw, event dispatch
 * @module wlt-widgets
 * @purpose Core widget management
 * @dependencies: widgets.h
 * @spec: docs/specs/wlt-compositor.md
 */

#include "widgets.h"
#include <string.h>
#include <stdlib.h>

static uint32_t g_next_id = 1;

wlt_widget_t *
wlt_widget_create(wlt_widget_type_t type)
{
    wlt_widget_t *w = (wlt_widget_t *)calloc(1, sizeof(wlt_widget_t));
    if (w == NULL)
        return NULL;

    w->id = g_next_id++;
    w->type = type;
    w->visible = true;
    w->width = 100;
    w->height = 30;
    w->bg_color = 0x1a1b26FF;
    w->fg_color = 0xC0CAF5FF;
    w->border_color = 0x7AA2F7FF;
    w->border_radius = 4;
    w->padding = 8;
    w->layout = WLT_W_LAYOUT_NONE;

    return w;
}

void
wlt_widget_destroy(wlt_widget_t *widget)
{
    if (widget == NULL)
        return;

    /* Destroy children first */
    for (size_t i = 0; i < widget->child_count; i++)
        wlt_widget_destroy(widget->children[i]);

    free(widget);
}

wlt_widget_error_t
wlt_widget_add_child(wlt_widget_t *parent, wlt_widget_t *child)
{
    if (parent == NULL || child == NULL)
        return WLT_W_ERR_INVALID_ARG;

    if (parent->child_count >= WLT_W_MAX_CHILDREN)
        return WLT_W_ERR_NOMEM;

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_remove_child(wlt_widget_t *parent, wlt_widget_t *child)
{
    if (parent == NULL || child == NULL)
        return WLT_W_ERR_INVALID_ARG;

    for (size_t i = 0; i < parent->child_count; i++)
    {
        if (parent->children[i] == child)
        {
            parent->children[i]->parent = NULL;
            for (size_t j = i + 1; j < parent->child_count; j++)
                parent->children[j - 1] = parent->children[j];
            parent->child_count--;
            return WLT_W_OK;
        }
    }

    return WLT_W_ERR_NOT_FOUND;
}

wlt_widget_error_t
wlt_widget_set_text(wlt_widget_t *widget, const char *text)
{
    if (widget == NULL || text == NULL)
        return WLT_W_ERR_INVALID_ARG;

    strncpy(widget->text, text, WLT_W_MAX_TEXT - 1);
    widget->text[WLT_W_MAX_TEXT - 1] = '\0';
    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_set_position(wlt_widget_t *widget, int32_t x, int32_t y)
{
    if (widget == NULL)
        return WLT_W_ERR_INVALID_ARG;
    widget->x = x;
    widget->y = y;
    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_set_size(wlt_widget_t *widget, int32_t w, int32_t h)
{
    if (widget == NULL || w < 0 || h < 0)
        return WLT_W_ERR_INVALID_ARG;
    widget->width = w;
    widget->height = h;
    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_set_color(wlt_widget_t *widget, uint32_t bg, uint32_t fg)
{
    if (widget == NULL)
        return WLT_W_ERR_INVALID_ARG;
    widget->bg_color = bg;
    widget->fg_color = fg;
    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_layout(wlt_widget_t *root)
{
    if (root == NULL)
        return WLT_W_ERR_INVALID_ARG;

    if (root->layout == WLT_W_LAYOUT_VBOX)
    {
        int32_t y = root->y + root->padding;
        for (size_t i = 0; i < root->child_count; i++)
        {
            wlt_widget_t *child = root->children[i];
            child->x = root->x + root->padding;
            child->y = y;
            child->width = root->width - 2 * root->padding;
            y += child->height + root->padding;
            wlt_widget_layout(child);
        }
    }
    else if (root->layout == WLT_W_LAYOUT_HBOX)
    {
        int32_t x = root->x + root->padding;
        for (size_t i = 0; i < root->child_count; i++)
        {
            wlt_widget_t *child = root->children[i];
            child->x = x;
            child->y = root->y + root->padding;
            child->height = root->height - 2 * root->padding;
            x += child->width + root->padding;
            wlt_widget_layout(child);
        }
    }
    else
    {
        for (size_t i = 0; i < root->child_count; i++)
            wlt_widget_layout(root->children[i]);
    }

    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_draw(wlt_widget_t *root)
{
    if (root == NULL)
        return WLT_W_ERR_INVALID_ARG;

    if (!root->visible)
        return WLT_W_OK;

    /* TODO(wlt-widgets): Call render functions:
     *   wlt_render_fill_rect(root->x, root->y, root->width, root->height, root->bg_color);
     *   if (root->border_radius > 0)
     *       wlt_render_rounded_rect(...);
     *   if (root->text[0])
     *       wlt_render_draw_text(root->x + root->padding, root->y + root->padding, root->text);
     */

    /* Draw children */
    for (size_t i = 0; i < root->child_count; i++)
        wlt_widget_draw(root->children[i]);

    return WLT_W_OK;
}

wlt_widget_error_t
wlt_widget_dispatch_event(wlt_widget_t *root, int32_t x, int32_t y, bool click)
{
    if (root == NULL || !root->visible)
        return WLT_W_OK;

    /* Check children first (top-most = last in array) */
    for (int i = (int)root->child_count - 1; i >= 0; i--)
    {
        wlt_widget_t *child = root->children[i];
        if (x >= child->x && x < child->x + child->width &&
            y >= child->y && y < child->y + child->height)
        {
            wlt_widget_error_t err = wlt_widget_dispatch_event(child, x, y, click);
            if (err == WLT_W_OK && click && child->on_click)
            {
                child->on_click(child);
                return WLT_W_OK;
            }
        }
    }

    /* Check self */
    if (click && root->type == WLT_W_TYPE_BUTTON &&
        x >= root->x && x < root->x + root->width &&
        y >= root->y && y < root->y + root->height)
    {
        if (root->on_click)
            root->on_click(root);
        return WLT_W_OK;
    }

    return WLT_W_OK;
}
