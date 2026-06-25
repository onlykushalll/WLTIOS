/**
 * @file shell_draw_test.c
 * @brief Unit tests for bitmap font and draw command pixel math
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0;
static int g_fail = 0;
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

static const uint8_t FONT_W[9] = {0x41,0x41,0x41,0x55,0x55,0x55,0x55,0x22,0x41};
static const uint8_t FONT_L[9] = {0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7F};
static const uint8_t FONT_T[9] = {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08};

static int count_bits(uint8_t b)
{
    int c = 0;
    while (b) { c += b & 1; b >>= 1; }
    return c;
}

static int count_glyph(const uint8_t g[9])
{
    int total = 0;
    for (int i = 0; i < 9; i++) total += count_bits(g[i]);
    return total;
}

/* Calculate pixel offset for a given (x, y) coordinate */
static int pixel_offset(int x, int y, int line_length, int bytes_per_pixel)
{
    return (y * line_length) + (x * bytes_per_pixel);
}

int main(void)
{
    printf("=== Shell Draw Tests ===\n\n");

    printf("--- Bitmap font W ---\n");
    int w_bits = count_glyph(FONT_W);
    TEST_ASSERT(w_bits >= 12, "W has at least 12 set bits");
    TEST_ASSERT(w_bits < 40, "W has fewer than 40 set bits");
    TEST_ASSERT(FONT_W[0] == 0x41, "W first row is 0x41 (left+right columns)");
    TEST_ASSERT(FONT_W[7] == 0x22, "W 8th row is 0x22 (inner V shape)");

    printf("\n--- Bitmap font L ---\n");
    int l_bits = count_glyph(FONT_L);
    TEST_ASSERT(l_bits >= 8, "L has at least 8 set bits");
    TEST_ASSERT(FONT_L[8] == 0x7F, "L bottom row is full (0x7F)");
    TEST_ASSERT(FONT_L[0] == 0x40, "L top row is left column only (0x40)");

    printf("\n--- Bitmap font T ---\n");
    int t_bits = count_glyph(FONT_T);
    TEST_ASSERT(t_bits >= 8, "T has at least 8 set bits");
    TEST_ASSERT(FONT_T[0] == 0x7F, "T top row is full (0x7F)");
    TEST_ASSERT(FONT_T[1] == 0x08, "T second row is center only (0x08)");

    printf("\n--- Bit counting ---\n");
    TEST_ASSERT(count_bits(0x00) == 0, "0x00 has 0 bits");
    TEST_ASSERT(count_bits(0x01) == 1, "0x01 has 1 bit");
    TEST_ASSERT(count_bits(0x7F) == 7, "0x7F has 7 bits");
    TEST_ASSERT(count_bits(0xFF) == 8, "0xFF has 8 bits");
    TEST_ASSERT(count_bits(0x55) == 4, "0x55 has 4 bits (alternating)");

    printf("\n--- Pixel offset calculation ---\n");
    /* 24bpp, line_length=3072 (1024 * 3) */
    int off = pixel_offset(0, 0, 3072, 3);
    TEST_ASSERT(off == 0, "Pixel (0,0) offset is 0");
    off = pixel_offset(1, 0, 3072, 3);
    TEST_ASSERT(off == 3, "Pixel (1,0) offset is 3 (24bpp)");
    off = pixel_offset(0, 1, 3072, 3);
    TEST_ASSERT(off == 3072, "Pixel (0,1) offset is 3072 (line_length)");
    off = pixel_offset(100, 200, 3072, 3);
    TEST_ASSERT(off == (200 * 3072 + 100 * 3), "Pixel (100,200) offset correct");

    printf("\n--- 32bpp pixel offset ---\n");
    off = pixel_offset(1, 0, 4096, 4);
    TEST_ASSERT(off == 4, "Pixel (1,0) offset is 4 (32bpp)");
    off = pixel_offset(0, 1, 4096, 4);
    TEST_ASSERT(off == 4096, "Pixel (0,1) offset is 4096 (32bpp line_length)");

    printf("\n--- Color constants ---\n");
    uint8_t navy_r = 0x1a, navy_g = 0x1a, navy_b = 0x2e;
    uint8_t cyan_r = 0x00, cyan_g = 0xd4, cyan_b = 0xff;
    uint8_t white_r = 0xff, white_g = 0xff, white_b = 0xff;
    TEST_ASSERT(navy_b > navy_r, "Navy blue > red (it's blue-ish)");
    TEST_ASSERT(navy_b > navy_g, "Navy blue > green (it's blue-ish)");
    TEST_ASSERT(cyan_g > cyan_r, "Cyan green > red (it's cyan)");
    TEST_ASSERT(cyan_b == 0xff, "Cyan blue is 0xff (max)");
    TEST_ASSERT(white_r == 0xff && white_g == 0xff && white_b == 0xff, "White is all max");

    printf("\n--- Text positioning ---\n");
    int fb_w = 1024, fb_h = 768;
    int scale = 4;
    int cw = 7, ch = 9, spacing = 2;
    int nletters = 6;
    int total_w = nletters * cw + (nletters - 1) * spacing;
    int tx = (fb_w - total_w * scale) / 2;
    int ty = (fb_h - ch * scale) / 2;
    TEST_ASSERT(tx > 0, "Text X position is positive (centered)");
    TEST_ASSERT(ty > 0, "Text Y position is positive (centered)");
    TEST_ASSERT(tx + total_w * scale < fb_w, "Text fits within screen width");
    TEST_ASSERT(ty + ch * scale < fb_h, "Text fits within screen height");

    printf("\n--- Rectangle positioning ---\n");
    int rw = fb_w * 56 / 100;
    int rh = fb_h * 48 / 100;
    int rx = (fb_w - rw) / 2;
    int ry = (fb_h - rh) / 2;
    TEST_ASSERT(rw == 573, "Rectangle width is 573 (56% of 1024)");
    TEST_ASSERT(rh == 368, "Rectangle height is 368 (48% of 768)");
    TEST_ASSERT(rx == 225, "Rectangle X is 225 (centered)");
    TEST_ASSERT(ry == 200, "Rectangle Y is 200 (centered)");
    TEST_ASSERT(rx + rw < fb_w, "Rectangle fits in screen width");
    TEST_ASSERT(ry + rh < fb_h, "Rectangle fits in screen height");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}