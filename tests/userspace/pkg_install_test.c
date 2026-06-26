/*
 * pkg_install_test.c — Test MANIFEST.toml parser
 *
 * Tests:
 *   - SHA256 field extracted from MANIFEST
 *   - Version field extracted
 *   - Name field extracted
 *
 * 6 assertions.
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

/* Minimal MANIFEST.toml parser */
typedef struct {
    char name[128];
    char version[32];
    char sha256[80];
} wlt_manifest_t;

static int parse_manifest(const char *toml, wlt_manifest_t *m) {
    memset(m, 0, sizeof(*m));
    const char *p;
    /* name = "hello-wlt" */
    p = strstr(toml, "name");
    if (p) {
        p = strchr(p, '"');
        if (p) {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < 127) m->name[i++] = *p++;
        }
    }
    /* version = "1.0.0" */
    p = strstr(toml, "version");
    if (p) {
        p = strchr(p, '"');
        if (p) {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < 31) m->version[i++] = *p++;
        }
    }
    /* sha256 = "abc123..." */
    p = strstr(toml, "sha256");
    if (p) {
        p = strchr(p, '"');
        if (p) {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < 79) m->sha256[i++] = *p++;
        }
    }
    return (m->name[0] && m->version[0]) ? 0 : -1;
}

int main(void) {
    printf("=== Package Install (MANIFEST parser) Test ===\n");

    /* Simulated MANIFEST.toml content */
    const char *manifest =
        "# WLTIOS Package Manifest\n"
        "name = \"hello-wlt\"\n"
        "version = \"1.0.0\"\n"
        "description = \"Hello World test package\"\n"
        "sha256 = \"a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef123456\"\n"
        "depends = []\n";

    printf("Test 1: Parse name field\n");
    {
        wlt_manifest_t m;
        int rc = parse_manifest(manifest, &m);
        ASSERT(rc == 0);
        ASSERT(strcmp(m.name, "hello-wlt") == 0);
    }

    printf("Test 2: Parse version field\n");
    {
        wlt_manifest_t m;
        parse_manifest(manifest, &m);
        ASSERT(strcmp(m.version, "1.0.0") == 0);
    }

    printf("Test 3: Parse sha256 field\n");
    {
        wlt_manifest_t m;
        parse_manifest(manifest, &m);
        ASSERT(strlen(m.sha256) == 64);
        ASSERT(strncmp(m.sha256, "a1b2c3d4", 8) == 0);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
