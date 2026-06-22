/**
 * @file render.h
 * @brief Public API for WLT-Render (rendering engine)
 * @module wlt-render
 * @purpose Draw to framebuffer — rectangles, text, images, effects
 * @dependencies: none
 * @tested_by: tests/userspace/render_test.c
 * @spec: docs/specs/wlt-compositor.md
 */

#ifndef WLT_RENDER_H
#define WLT_RENDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WLT_R_OK = 0,
    WLT_R_ERR_INVALID_ARG = -1,
    WLT_R_ERR_IO = -2,
    WLT_R_ERR_NOMEM = -3,
} wlt_render_error_t;

#define WLT_R_DEFAULT_WIDTH 1366
#define WLT_R_DEFAULT_HEIGHT 768
#define WLT_R_BPP 4  /* bytes per pixel (BGRA) */

typedef struct {
    uint8_t *buffer;
    int32_t width;
    int32_t height;
    int32_t stride;
    bool dirty;
} wlt_render_context_t;

wlt_render_error_t wlt_render_init(wlt_render_context_t *ctx, int32_t width, int32_t height);
void wlt_render_shutdown(wlt_render_context_t *ctx);

wlt_render_error_t wlt_render_clear(wlt_render_context_t *ctx, uint32_t color);
wlt_render_error_t wlt_render_fill_rect(wlt_render_context_t *ctx, int32_t x, int32_t y,
                                         int32_t w, int32_t h, uint32_t color);
wlt_render_error_t wlt_render_draw_rect(wlt_render_context_t *ctx, int32_t x, int32_t y,
                                         int32_t w, int32_t h, uint32_t color, int32_t thickness);
wlt_render_error_t wlt_render_draw_line(wlt_render_context_t *ctx, int32_t x1, int32_t y1,
                                         int32_t x2, int32_t y2, uint32_t color);
wlt_render_error_t wlt_render_draw_text(wlt_render_context_t *ctx, int32_t x, int32_t y,
                                         const char *text, uint32_t color);
wlt_render_error_t wlt_render_flush(wlt_render_context_t *ctx);

/* Color helpers (BGRA format: 0xAARRGGBB) */
static inline uint32_t wlt_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint8_t wlt_color_r(uint32_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t wlt_color_g(uint32_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t wlt_color_b(uint32_t c) { return c & 0xFF; }
static inline uint8_t wlt_color_a(uint32_t c) { return (c >> 24) & 0xFF; }

#ifdef __cplusplus
}
#endif

#endif /* WLT_RENDER_H */
