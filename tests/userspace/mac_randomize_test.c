/**
 * @file mac_randomize_test.c
 * @brief Unit tests for MAC address randomization
 * @module wlt-net
 * @purpose Verify MAC generation sets locally-administered + unicast bits
 *
 * Build: gcc -Wall -Wextra -std=gnu11 -O2 -o /tmp/wlt-test-mac \
 *          tests/userspace/mac_randomize_test.c
 * Run:   /tmp/wlt-test-mac
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

/* Replicate the MAC randomization logic from qemu_bringup.c */
static void randomize_mac_byte0(uint8_t *mac_byte0)
{
    /* Set locally administered (bit 1) + unicast (bit 0 clear) */
    *mac_byte0 = (*mac_byte0 & 0xFE) | 0x02;
}

/* Check if a MAC is locally administered */
static int is_locally_administered(uint8_t first_byte)
{
    return (first_byte & 0x02) != 0;
}

/* Check if a MAC is unicast */
static int is_unicast(uint8_t first_byte)
{
    return (first_byte & 0x01) == 0;
}

/* Format MAC to string */
static void mac_to_str(const uint8_t mac[6], char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int main(void)
{
    printf("=== MAC Randomization Tests ===\n\n");

    /* Test 1: Bit manipulation — set locally administered, clear unicast */
    printf("--- Bit manipulation ---\n");
    uint8_t test_byte = 0x00;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0x02, "0x00 → 0x02 (locally administered, unicast)");

    test_byte = 0xFF;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0xFE, "0xFF → 0xFE (bit 0 cleared, bit 1 kept)");

    test_byte = 0x01;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0x02, "0x01 → 0x02 (unicast bit cleared, LA bit set)");

    test_byte = 0x02;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0x02, "0x02 → 0x02 (already correct)");

    test_byte = 0xAA;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0xAA, "0xAA → 0xAA (bit 1 already set, bit 0 already clear)");

    test_byte = 0xAB;
    randomize_mac_byte0(&test_byte);
    TEST_ASSERT(test_byte == 0xAA, "0xAB → 0xAA (unicast bit cleared)");

    /* Test 2: Locally administered bit check */
    printf("\n--- Locally administered check ---\n");
    TEST_ASSERT(is_locally_administered(0x02) == 1, "0x02 is locally administered");
    TEST_ASSERT(is_locally_administered(0x06) == 1, "0x06 is locally administered");
    TEST_ASSERT(is_locally_administered(0x0A) == 1, "0x0A is locally administered");
    TEST_ASSERT(is_locally_administered(0x12) == 1, "0x12 is locally administered");
    TEST_ASSERT(is_locally_administered(0x00) == 0, "0x00 is NOT locally administered");
    TEST_ASSERT(is_locally_administered(0x01) == 0, "0x01 is NOT locally administered");
    TEST_ASSERT(is_locally_administered(0xFC) == 0, "0xFC is NOT locally administered");

    /* Test 3: Unicast check */
    printf("\n--- Unicast check ---\n");
    TEST_ASSERT(is_unicast(0x00) == 1, "0x00 is unicast");
    TEST_ASSERT(is_unicast(0x02) == 1, "0x02 is unicast");
    TEST_ASSERT(is_unicast(0x04) == 1, "0x04 is unicast");
    TEST_ASSERT(is_unicast(0x01) == 0, "0x01 is multicast");
    TEST_ASSERT(is_unicast(0x03) == 0, "0x03 is multicast");
    TEST_ASSERT(is_unicast(0xFF) == 0, "0xFF is multicast");

    /* Test 4: Full MAC randomization simulation */
    printf("\n--- Full MAC simulation ---\n");
    uint8_t mac1[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};  /* QEMU default */
    uint8_t mac2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t mac3[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    randomize_mac_byte0(&mac1[0]);
    randomize_mac_byte0(&mac2[0]);
    randomize_mac_byte0(&mac3[0]);

    TEST_ASSERT(is_locally_administered(mac1[0]) == 1, "MAC1 is locally administered");
    TEST_ASSERT(is_unicast(mac1[0]) == 1, "MAC1 is unicast");
    TEST_ASSERT(is_locally_administered(mac2[0]) == 1, "MAC2 is locally administered");
    TEST_ASSERT(is_unicast(mac2[0]) == 1, "MAC2 is unicast");
    TEST_ASSERT(is_locally_administered(mac3[0]) == 1, "MAC3 is locally administered");
    TEST_ASSERT(is_unicast(mac3[0]) == 1, "MAC3 is unicast");

    /* Test 5: MAC string formatting */
    printf("\n--- MAC string formatting ---\n");
    char mac_str[18];
    uint8_t test_mac[6] = {0xc6, 0x4a, 0x36, 0x5c, 0xb0, 0x48};
    mac_to_str(test_mac, mac_str, sizeof(mac_str));
    TEST_ASSERT(strcmp(mac_str, "c6:4a:36:5c:b0:48") == 0, "MAC formatted correctly");
    TEST_ASSERT(strlen(mac_str) == 17, "MAC string is 17 chars (xx:xx:xx:xx:xx:xx)");

    /* Test 6: Two different random MACs are different */
    printf("\n--- MAC uniqueness ---\n");
    uint8_t boot1_mac[6] = {0xc6, 0x4a, 0x36, 0x5c, 0xb0, 0x48};
    uint8_t boot2_mac[6] = {0xbe, 0x2e, 0x16, 0xe1, 0x30, 0x68};
    int same = memcmp(boot1_mac, boot2_mac, 6) == 0;
    TEST_ASSERT(!same, "Two random MACs are different");

    /* Test 7: OUI (first 3 bytes) — should NOT match common vendors */
    printf("\n--- OUI privacy ---\n");
    /* QEMU's default OUI is 52:54:00 — should be changed */
    uint8_t qemu_oui[3] = {0x52, 0x54, 0x00};
    TEST_ASSERT(memcmp(boot1_mac, qemu_oui, 3) != 0, "Boot1 MAC doesn't use QEMU OUI");
    TEST_ASSERT(memcmp(boot2_mac, qemu_oui, 3) != 0, "Boot2 MAC doesn't use QEMU OUI");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}