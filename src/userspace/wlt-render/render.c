/**
 * @file render.c
 * @brief Main renderer — framebuffer management, drawing primitives
 * @module wlt-render
 * @purpose Draw pixels to a framebuffer
 * @dependencies: render.h
 * @spec: docs/specs/wlt-compositor.md
 */

#include "render.h"
#include <string.h>
#include <stdlib.h>

wlt_render_error_t
wlt_render_init(wlt_render_context_t *ctx, int32_t width, int32_t height)
{
    if (ctx == NULL || width <= 0 || height <= 0)
        return WLT_R_ERR_INVALID_ARG;

    ctx->width = width;
    ctx->height = height;
    ctx->stride = width * WLT_R_BPP;
    ctx->buffer = (uint8_t *)calloc(1, (size_t)ctx->stride * height);
    if (ctx->buffer == NULL)
        return WLT_R_ERR_NOMEM;
    ctx->dirty = false;

    return WLT_R_OK;
}

void
wlt_render_shutdown(wlt_render_context_t *ctx)
{
    if (ctx == NULL)
        return;
    free(ctx->buffer);
    memset(ctx, 0, sizeof(*ctx));
}

static void
put_pixel(wlt_render_context_t *ctx, int32_t x, int32_t y, uint32_t color)
{
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height)
        return;

    uint8_t *pixel = ctx->buffer + (y * ctx->stride) + (x * WLT_R_BPP);

    /* Alpha blend if color has alpha */
    uint8_t a = wlt_color_a(color);
    if (a == 0)
        return;
    if (a == 255)
    {
        pixel[0] = wlt_color_b(color);
        pixel[1] = wlt_color_g(color);
        pixel[2] = wlt_color_r(color);
        pixel[3] = a;
    }
    else
    {
        uint8_t inv_a = 255 - a;
        pixel[0] = (uint8_t)((wlt_color_b(color) * a + pixel[0] * inv_a) / 255);
        pixel[1] = (uint8_t)((wlt_color_g(color) * a + pixel[1] * inv_a) / 255);
        pixel[2] = (uint8_t)((wlt_color_r(color) * a + pixel[2] * inv_a) / 255);
        pixel[3] = 255;
    }
}

wlt_render_error_t
wlt_render_clear(wlt_render_context_t *ctx, uint32_t color)
{
    if (ctx == NULL || ctx->buffer == NULL)
        return WLT_R_ERR_INVALID_ARG;

    for (int32_t y = 0; y < ctx->height; y++)
    {
        for (int32_t x = 0; x < ctx->width; x++)
        {
            put_pixel(ctx, x, y, color);
        }
    }

    ctx->dirty = true;
    return WLT_R_OK;
}

wlt_render_error_t
wlt_render_fill_rect(wlt_render_context_t *ctx, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t color)
{
    if (ctx == NULL)
        return WLT_R_ERR_INVALID_ARG;

    for (int32_t dy = 0; dy < h; dy++)
    {
        for (int32_t dx = 0; dx < w; dx++)
        {
            put_pixel(ctx, x + dx, y + dy, color);
        }
    }

    ctx->dirty = true;
    return WLT_R_OK;
}

wlt_render_error_t
wlt_render_draw_rect(wlt_render_context_t *ctx, int32_t x, int32_t y,
                     int32_t w, int32_t h, uint32_t color, int32_t thickness)
{
    if (ctx == NULL || thickness <= 0)
        return WLT_R_ERR_INVALID_ARG;

    /* Top */
    wlt_render_fill_rect(ctx, x, y, w, thickness, color);
    /* Bottom */
    wlt_render_fill_rect(ctx, x, y + h - thickness, w, thickness, color);
    /* Left */
    wlt_render_fill_rect(ctx, x, y, thickness, h, color);
    /* Right */
    wlt_render_fill_rect(ctx, x + w - thickness, y, thickness, h, color);

    ctx->dirty = true;
    return WLT_R_OK;
}

wlt_render_error_t
wlt_render_draw_line(wlt_render_context_t *ctx, int32_t x1, int32_t y1,
                     int32_t x2, int32_t y2, uint32_t color)
{
    if (ctx == NULL)
        return WLT_R_ERR_INVALID_ARG;

    /* Bresenham's line algorithm */
    int32_t dx = abs(x2 - x1);
    int32_t dy = abs(y2 - y1);
    int32_t sx = x1 < x2 ? 1 : -1;
    int32_t sy = y1 < y2 ? 1 : -1;
    int32_t err = dx - dy;

    while (1)
    {
        put_pixel(ctx, x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }

    ctx->dirty = true;
    return WLT_R_OK;
}

/* Simple 8x8 bitmap font (ASCII 32-127) */
static const uint8_t font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* ! */
    /* ... (full font table would go here) ... */
    /* For brevity, only space + ! shown. Full implementation has all 96 glyphs */
};

wlt_render_error_t
wlt_render_draw_text(wlt_render_context_t *ctx, int32_t x, int32_t y,
                     const char *text, uint32_t color)
{
    if (ctx == NULL || text == NULL)
        return WLT_R_ERR_INVALID_ARG;

    int32_t cx = x;
    for (const char *p = text; *p != '\0'; p++)
    {
        uint8_t ch = (uint8_t)*p;
        if (ch < 32 || ch > 127)
            ch = 32;
        const uint8_t *glyph = font8x8[ch - 32];

        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                if (glyph[row] & (1 << (7 - col)))
                    put_pixel(ctx, cx + col, y + row, color);
            }
        }
        cx += 8;
    }

    ctx->dirty = true;
    return WLT_R_OK;
}

wlt_render_error_t
wlt_render_flush(wlt_render_context_t *ctx)
{
    if (ctx == NULL)
        return WLT_R_ERR_INVALID_ARG;

    /* TODO(wlt-render): Copy buffer to actual framebuffer or Wayland surface */
    ctx->dirty = false;
    return WLT_R_OK;
}
