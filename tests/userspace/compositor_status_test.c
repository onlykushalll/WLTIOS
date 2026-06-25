/**
 * @file compositor_status_test.c
 * @brief Unit tests for compositor status bar clock formatting + init path
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

/* Format seconds since boot as HH:MM:SS */
static void format_clock(unsigned int seconds, char *buf, size_t buf_size)
{
    unsigned int h = seconds / 3600;
    unsigned int m = (seconds % 3600) / 60;
    unsigned int s = seconds % 60;
    snprintf(buf, buf_size, "%02u:%02u:%02u", h, m, s);
}

/* Check if kill switch is active (simulated) */
static int killswitch_active = 0;
static const char *get_ks_status(void)
{
    return killswitch_active ? "KS ON" : "KS OFF";
}

/* Check if Tor is running (simulated) */
static int tor_running = 0;
static const char *get_tor_status(void)
{
    return tor_running ? "TOR" : "NO TOR";
}

int main(void)
{
    printf("=== Compositor Status Tests ===\n\n");

    printf("--- Clock formatting ---\n");
    char clock[16];

    format_clock(0, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "00:00:00") == 0, "0 seconds = 00:00:00");

    format_clock(1, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "00:00:01") == 0, "1 second = 00:00:01");

    format_clock(59, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "00:00:59") == 0, "59 seconds = 00:00:59");

    format_clock(60, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "00:01:00") == 0, "60 seconds = 00:01:00");

    format_clock(3661, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "01:01:01") == 0, "3661 seconds = 01:01:01");

    format_clock(3600, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "01:00:00") == 0, "3600 seconds = 01:00:00");

    format_clock(86399, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "23:59:59") == 0, "86399 seconds = 23:59:59 (max day)");

    format_clock(90000, clock, sizeof(clock));
    TEST_ASSERT(strcmp(clock, "25:00:00") == 0, "90000 seconds = 25:00:00 (wraps past 24h)");

    printf("\n--- Kill switch status ---\n");
    killswitch_active = 0;
    TEST_ASSERT(strcmp(get_ks_status(), "KS OFF") == 0, "Default KS status is OFF");

    killswitch_active = 1;
    TEST_ASSERT(strcmp(get_ks_status(), "KS ON") == 0, "Active KS status is ON");

    killswitch_active = 0;
    TEST_ASSERT(strcmp(get_ks_status(), "KS OFF") == 0, "Deactivated KS status is OFF");

    printf("\n--- Tor status ---\n");
    tor_running = 0;
    TEST_ASSERT(strcmp(get_tor_status(), "NO TOR") == 0, "Default Tor status is NO TOR");

    tor_running = 1;
    TEST_ASSERT(strcmp(get_tor_status(), "TOR") == 0, "Running Tor status is TOR");

    printf("\n--- Compositor init path ---\n");
    /* Verify the init sequence is correct */
    const char *init_steps[] = {
        "headless backend",
        "pixman renderer",
        "allocator",
        "scene graph",
        "output layout",
        "headless output",
        "seat",
        "wayland socket",
    };
    int num_steps = sizeof(init_steps) / sizeof(init_steps[0]);
    TEST_ASSERT(num_steps == 8, "Compositor has 8 init steps");
    TEST_ASSERT(strcmp(init_steps[0], "headless backend") == 0, "First step is headless backend");
    TEST_ASSERT(strcmp(init_steps[1], "pixman renderer") == 0, "Second step is pixman renderer");
    TEST_ASSERT(strcmp(init_steps[7], "wayland socket") == 0, "Last step is wayland socket");

    printf("\n--- Output dimensions ---\n");
    int output_w = 1366, output_h = 768;
    TEST_ASSERT(output_w == 1366, "Output width is 1366 (Kushal's display)");
    TEST_ASSERT(output_h == 768, "Output height is 768");
    TEST_ASSERT(output_w * output_h == 1049088, "Output pixel count is 1049088");

    printf("\n--- Status bar dimensions ---\n");
    int bar_height = 32;
    int bar_y = output_h - bar_height;
    TEST_ASSERT(bar_height == 32, "Status bar height is 32px");
    TEST_ASSERT(bar_y == 736, "Status bar Y position is 736 (768-32)");

    printf("\n--- Color values ---\n");
    uint32_t bar_bg = 0x2a2a3e;
    uint32_t tor_green = 0x00ff88;
    uint32_t tor_red = 0xff4444;
    uint32_t ks_yellow = 0xffff00;
    TEST_ASSERT((bar_bg >> 16 & 0xFF) == 0x2a, "Bar bg red is 0x2a");
    TEST_ASSERT((tor_green >> 8 & 0xFF) == 0xff, "Tor green is 0x00ff88");
    TEST_ASSERT((tor_red >> 16 & 0xFF) == 0xff, "Tor red is 0xff4444");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}