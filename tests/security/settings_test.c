/*
 * tests/userspace/settings_test.c — Settings data parsers
 * 5 assertions.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) do { \
    test_count++; \
    if (cond) { pass_count++; printf("  [PASS] %s\n", #cond); } \
    else { printf("  [FAIL] %s\n", #cond); } \
} while(0)

/* Parse /proc/net/route entry to extract gateway IP */
static int parse_route_gw(const char *line, char *gw_out, size_t gw_len) {
    char iface[32];
    unsigned int dest, gw, mask, flags;
    if (sscanf(line, "%31s %x %x %x %x", iface, &dest, &gw, &mask, &flags) >= 5) {
        if (dest == 0) {
            snprintf(gw_out, gw_len, "%d.%d.%d.%d",
                (gw >> 0) & 0xFF, (gw >> 8) & 0xFF,
                (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
            return 0;
        }
    }
    return -1;
}

/* Parse /etc/wltios-release to extract version */
static int parse_version(const char *content, char *ver_out, size_t ver_len) {
    const char *p = strstr(content, "WLTIOS_VERSION=");
    if (!p) return -1;
    p += 15;  /* skip "WLTIOS_VERSION=" */
    size_t i = 0;
    while (p[i] && p[i] != '\n' && i < ver_len - 1) {
        ver_out[i] = p[i];
        i++;
    }
    ver_out[i] = 0;
    return 0;
}

int main(void) {
    printf("=== Settings Parser Test ===\n");

    /* Test 1: Parse default route gateway */
    printf("Test 1: Parse route gateway\n");
    {
        const char *route_line = "eth0\t00000000\t0202000A\t0003\t0\t0\t0\t00000000\t0\t0\t0";
        char gw[32];
        int rc = parse_route_gw(route_line, gw, sizeof(gw));
        ASSERT(rc == 0);
        ASSERT(strcmp(gw, "10.0.2.2") == 0);
    }

    /* Test 2: Parse non-default route (should fail) */
    printf("Test 2: Non-default route\n");
    {
        const char *route_line = "eth0\t0002000A\t00000000\t0001\t0\t0\t0\t00FFFFFF\t0\t0\t0";
        char gw[32];
        int rc = parse_route_gw(route_line, gw, sizeof(gw));
        ASSERT(rc != 0);
    }

    /* Test 3: Parse wltios-release version */
    printf("Test 3: Parse version\n");
    {
        const char *release = "WLTIOS_VERSION=0.1.0\n";
        char ver[32];
        int rc = parse_version(release, ver, sizeof(ver));
        ASSERT(rc == 0);
        ASSERT(strcmp(ver, "0.1.0") == 0);
    }

    /* Test 4: Parse version from multi-line content */
    printf("Test 4: Multi-line version\n");
    {
        const char *release = "NAME=WLTIOS\nWLTIOS_VERSION=0.2.0\nKERNEL=5.10.0\n";
        char ver[32];
        int rc = parse_version(release, ver, sizeof(ver));
        ASSERT(rc == 0);
        ASSERT(strcmp(ver, "0.2.0") == 0);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
