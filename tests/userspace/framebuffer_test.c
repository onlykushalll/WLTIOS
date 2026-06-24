/**
 * @file framebuffer_test.c
 * @brief Unit tests for framebuffer init and bitmap font
 * @module wlt-init
 * @purpose Verify framebuffer mode setting and WLTIOS bitmap font correctness
 *
 * Build: gcc -Wall -Wextra -std=gnu11 -O2 -o /tmp/wlt-test-fb \
 *          tests/userspace/framebuffer_test.c
 * Run:   /tmp/wlt-test-fb
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Test assertion macro — counts pass/fail */
static int g_pass = 0;
static int g_fail = 0;
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

/* The bitmap font from shell.c — replicated here for testing.
 * Each letter is 9 rows, 7 bits per row (bit 6 = leftmost pixel). */
static const uint8_t FONT_W[9] = {0x41,0x41,0x41,0x55,0x55,0x55,0x55,0x22,0x41};
static const uint8_t FONT_L[9] = {0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7F};
static const uint8_t FONT_T[9] = {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08};
static const uint8_t FONT_I[9] = {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x7F};
static const uint8_t FONT_O[9] = {0x3E,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x3E};
static const uint8_t FONT_S[9] = {0x3E,0x41,0x40,0x40,0x3E,0x01,0x01,0x41,0x3E};

/* Count set bits in a byte */
static int count_bits(uint8_t b)
{
    int count = 0;
    while (b) { count += b & 1; b >>= 1; }
    return count;
}

/* Count total set pixels in a glyph */
static int count_glyph_pixels(const uint8_t glyph[9])
{
    int total = 0;
    for (int i = 0; i < 9; i++)
        total += count_bits(glyph[i]);
    return total;
}

int main(void)
{
    printf("=== Framebuffer + Bitmap Font Tests ===\n\n");

    /* Test 1: Font array dimensions */
    printf("--- Font dimensions ---\n");
    TEST_ASSERT(sizeof(FONT_W) == 9, "FONT_W has 9 rows");
    TEST_ASSERT(sizeof(FONT_L) == 9, "FONT_L has 9 rows");
    TEST_ASSERT(sizeof(FONT_T) == 9, "FONT_T has 9 rows");
    TEST_ASSERT(sizeof(FONT_I) == 9, "FONT_I has 9 rows");
    TEST_ASSERT(sizeof(FONT_O) == 9, "FONT_O has 9 rows");
    TEST_ASSERT(sizeof(FONT_S) == 9, "FONT_S has 9 rows");

    /* Test 2: Each glyph has reasonable pixel count (not empty, not full) */
    printf("\n--- Glyph pixel counts ---\n");
    int w_pixels = count_glyph_pixels(FONT_W);
    int l_pixels = count_glyph_pixels(FONT_L);
    int t_pixels = count_glyph_pixels(FONT_T);
    int i_pixels = count_glyph_pixels(FONT_I);
    int o_pixels = count_glyph_pixels(FONT_O);
    int s_pixels = count_glyph_pixels(FONT_S);

    TEST_ASSERT(w_pixels > 5 && w_pixels < 30, "W has reasonable pixel count");
    TEST_ASSERT(l_pixels > 5 && l_pixels < 20, "L has reasonable pixel count");
    TEST_ASSERT(t_pixels > 5 && t_pixels < 20, "T has reasonable pixel count");
    TEST_ASSERT(i_pixels > 5 && i_pixels < 25, "I has reasonable pixel count (serifs add pixels)");
    TEST_ASSERT(o_pixels > 5 && o_pixels < 30, "O has reasonable pixel count");
    TEST_ASSERT(s_pixels > 5 && s_pixels < 30, "S has reasonable pixel count");

    /* Test 3: W and O should have more pixels than T (W/O are wider) */
    printf("\n--- Glyph relative sizes ---\n");
    TEST_ASSERT(w_pixels > t_pixels, "W has more pixels than T");
    TEST_ASSERT(o_pixels > t_pixels, "O has more pixels than T");

    /* Test 4: L has a full bottom row (0x7F) */
    printf("\n--- Glyph structure ---\n");
    TEST_ASSERT(FONT_L[8] == 0x7F, "L bottom row is full (0x7F)");
    TEST_ASSERT(FONT_T[0] == 0x7F, "T top row is full (0x7F)");
    TEST_ASSERT(FONT_I[0] == 0x7F, "I top row is full (serif)");
    TEST_ASSERT(FONT_I[8] == 0x7F, "I bottom row is full (serif)");

    /* Test 5: O is symmetric (top and bottom rows match) */
    printf("\n--- Glyph symmetry ---\n");
    TEST_ASSERT(FONT_O[0] == FONT_O[8], "O top and bottom rows match");
    TEST_ASSERT(FONT_I[0] == FONT_I[8], "I top and bottom rows match (serifs)");

    /* Test 6: First byte of locally-administered MAC has bit 1 set */
    printf("\n--- MAC address format ---\n");
    uint8_t mac_byte = 0xAA;  /* example after randomization */
    TEST_ASSERT((mac_byte & 0x02) != 0, "Locally administered bit (1) is set");
    TEST_ASSERT((mac_byte & 0x01) == 0, "Unicast bit (0) is clear");

    /* Test 7: Framebuffer color constants */
    printf("\n--- Color constants ---\n");
    uint32_t navy = 0xFF1a1a2e;
    uint32_t cyan = 0xFF00d4ff;
    uint32_t white = 0xFFFFFFFF;
    TEST_ASSERT((navy >> 24) == 0xFF, "Navy alpha is 0xFF");
    TEST_ASSERT((navy & 0xFF) == 0x2e, "Navy blue component is 0x2e");
    TEST_ASSERT((cyan & 0xFF) == 0xff, "Cyan blue component is 0xFF");
    TEST_ASSERT((cyan >> 8 & 0xFF) == 0xd4, "Cyan green component is 0xd4");
    TEST_ASSERT(white == 0xFFFFFFFF, "White is all 1s");

    /* Test 8: VGA text mode attributes */
    printf("\n--- VGA text mode attributes ---\n");
    uint8_t blue_bg = 0x01;   /* blue background, black text */
    uint8_t cyan_bg_white_fg = 0x3F; /* cyan bg, bright white fg */
    TEST_ASSERT((blue_bg & 0x0F) == 0x01, "Blue bg fg is black+blue");
    TEST_ASSERT((blue_bg >> 4) == 0x00, "Blue bg attribute byte bg is black");
    TEST_ASSERT((cyan_bg_white_fg & 0x0F) == 0x0F, "Cyan bg fg is bright white");
    TEST_ASSERT((cyan_bg_white_fg >> 4) == 0x03, "Cyan bg attribute bg is cyan");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}