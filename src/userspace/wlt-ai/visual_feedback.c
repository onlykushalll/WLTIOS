/**
 * @file visual_feedback.c
 * @brief Visual feedback — highlight screen elements Kairos is referring to
 * @module wlt-ai/visual_feedback
 * @purpose Draw attention rectangles, arrows, dimming overlays on screen
 */
#include "ai.h"
#include <string.h>
#include <stdio.h>

typedef enum {
    WLT_VF_NONE = 0,
    WLT_VF_HIGHLIGHT,    /* Rectangle around an element */
    WLT_VF_ARROW,        /* Arrow pointing at something */
    WLT_VF_DIM,          /* Dim everything except highlighted area */
    WLT_VF_PULSE,        /* Pulsing circle around something */
} wlt_vf_type_t;

typedef struct {
    wlt_vf_type_t type;
    int32_t x, y, width, height;
    uint32_t color;      /* ARGB */
    int32_t duration_ms;  /* 0 = until cleared */
    int64_t start_time;
    bool active;
} wlt_vf_overlay_t;

#define WLT_VF_MAX_OVERLAYS 8
static wlt_vf_overlay_t g_overlays[WLT_VF_MAX_OVERLAYS] = {0};

wlt_ai_err_t
wlt_vf_highlight(int32_t x, int32_t y, int32_t w, int32_t h,
                 uint32_t color, int32_t duration_ms)
{
    for (int i = 0; i < WLT_VF_MAX_OVERLAYS; i++) {
        if (!g_overlays[i].active) {
            g_overlays[i].type = WLT_VF_HIGHLIGHT;
            g_overlays[i].x = x;
            g_overlays[i].y = y;
            g_overlays[i].width = w;
            g_overlays[i].height = h;
            g_overlays[i].color = color;
            g_overlays[i].duration_ms = duration_ms;
            g_overlays[i].active = true;
            return WLT_AI_OK;
        }
    }
    return WLT_AI_ERR_NOMEM;
}

wlt_ai_err_t
wlt_vf_pulse(int32_t x, int32_t y, int32_t radius,
             uint32_t color, int32_t duration_ms)
{
    for (int i = 0; i < WLT_VF_MAX_OVERLAYS; i++) {
        if (!g_overlays[i].active) {
            g_overlays[i].type = WLT_VF_PULSE;
            g_overlays[i].x = x - radius;
            g_overlays[i].y = y - radius;
            g_overlays[i].width = radius * 2;
            g_overlays[i].height = radius * 2;
            g_overlays[i].color = color;
            g_overlays[i].duration_ms = duration_ms;
            g_overlays[i].active = true;
            return WLT_AI_OK;
        }
    }
    return WLT_AI_ERR_NOMEM;
}

wlt_ai_err_t
wlt_vf_clear_all(void)
{
    memset(g_overlays, 0, sizeof(g_overlays));
    return WLT_AI_OK;
}

wlt_ai_err_t
wlt_vf_render(void *render_ctx)
{
    (void)render_ctx;
    /* In production: use wlt-render to draw overlays:
     * - HIGHLIGHT: draw rectangle border (2px, semi-transparent fill)
     * - PULSE: animated expanding circle (alpha fade)
     * - ARROW: draw line + arrowhead
     * - DIM: fill screen with 50% black, except highlighted rect (cutout)
     */
    for (int i = 0; i < WLT_VF_MAX_OVERLAYS; i++) {
        if (g_overlays[i].active) {
            /* Draw overlay (placeholder — real rendering in compositor) */
        }
    }
    return WLT_AI_OK;
}

int
wlt_vf_active_count(void)
{
    int count = 0;
    for (int i = 0; i < WLT_VF_MAX_OVERLAYS; i++)
        if (g_overlays[i].active) count++;
    return count;
}
