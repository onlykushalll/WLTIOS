/**
 * @file sha256_test.c
 * @brief Unit tests for SHA-256 implementation in WLT-Boot
 * @module wlt-boot/verify
 * @purpose Verify SHA-256 produces correct hashes (NIST FIPS 180-4 test vectors)
 * @spec: docs/specs/wlt-boot.md
 *
 * Build: gcc -o sha256_test sha256_test.c ../../src/boot/wlt-boot/verify.c -I../../src/boot/wlt-boot/include
 * Run:   ./sha256_test
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Include the verify header */
#include "../../src/boot/wlt-boot/include/verify.h"

struct test_case {
    const char *input;
    const char *expected_hash;
};

static struct test_case test_cases[] = {
    {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {"message digest", "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
};

static int
hex_compare(const uint8_t *hash, const char *expected_hex)
{
    for (int i = 0; i < 32; i++)
    {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        if (hex[0] != tolower(expected_hex[i * 2]) ||
            hex[1] != tolower(expected_hex[i * 2 + 1]))
        {
            return -1;
        }
    }
    return 0;
}

static void
print_hash(const uint8_t *hash)
{
    for (int i = 0; i < 32; i++)
        printf("%02x", hash[i]);
}

int
main(void)
{
    int pass = 0, fail = 0;
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    printf("=== SHA-256 Test Suite (NIST FIPS 180-4) ===\n\n");

    for (int i = 0; i < num_tests; i++)
    {
        uint8_t hash[32];
        wlt_verify_sha256((const uint8_t *)test_cases[i].input,
                          strlen(test_cases[i].input), hash);

        if (hex_compare(hash, test_cases[i].expected_hash) == 0)
        {
            printf("[PASS] Test %d\n", i + 1);
            pass++;
        }
        else
        {
            printf("[FAIL] Test %d\n", i + 1);
            printf("  Expected: %s\n", test_cases[i].expected_hash);
            printf("  Got:      ");
            print_hash(hash);
            printf("\n");
            fail++;
        }
    }

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
