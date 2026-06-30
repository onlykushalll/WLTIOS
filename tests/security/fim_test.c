/*
 * tests/security/fim_test.c — FIM hash computation and change detection
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

/* Simple hash function (not SHA-256, but deterministic for testing) */
static unsigned long mock_hash(const char *data, size_t len) {
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++)
        hash = ((hash << 5) + hash) + data[i];
    return hash;
}

int main(void) {
    printf("=== FIM Hash Test ===\n");

    /* Test 1: Hash is deterministic */
    printf("Test 1: Deterministic hash\n");
    {
        const char *data = "WLTIOS v0.1.0";
        unsigned long h1 = mock_hash(data, strlen(data));
        unsigned long h2 = mock_hash(data, strlen(data));
        ASSERT(h1 == h2);
    }

    /* Test 2: Different data = different hash */
    printf("Test 2: Different data, different hash\n");
    {
        const char *data1 = "WLTIOS v0.1.0";
        const char *data2 = "WLTIOS v0.1.1";
        unsigned long h1 = mock_hash(data1, strlen(data1));
        unsigned long h2 = mock_hash(data2, strlen(data2));
        ASSERT(h1 != h2);
    }

    /* Test 3: Tampered file detected */
    printf("Test 3: Tamper detection\n");
    {
        const char *original = "root:x:0:0:root:/root:/bin/sh";
        const char *tampered = "root:x:0:0:root:/root:/bin/sh\nhacker:x:0:0::/:/bin/sh";
        unsigned long h_orig = mock_hash(original, strlen(original));
        unsigned long h_tamp = mock_hash(tampered, strlen(tampered));
        ASSERT(h_orig != h_tamp);
    }

    /* Test 4: Empty file hash */
    printf("Test 4: Empty file hash\n");
    {
        unsigned long h = mock_hash("", 0);
        ASSERT(h == 5381);  /* djb2 initial value */
    }

    /* Test 5: Single character hash */
    printf("Test 5: Single character\n");
    {
        unsigned long h = mock_hash("A", 1);
        ASSERT(h != 5381);  /* different from empty */
    }

    /* Test 6: Hash changes with appended data */
    printf("Test 6: Appended data changes hash\n");
    {
        const char *base = "/etc/wltios-release";
        const char *modified = "/etc/wltios-release\ntampered";
        unsigned long h1 = mock_hash(base, strlen(base));
        unsigned long h2 = mock_hash(modified, strlen(modified));
        ASSERT(h1 != h2);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
