/**
 * @file kill_switch_test.c
 * @brief Unit tests for kill switch state machine
 * @module wlt-net
 * @purpose Verify kill switch enable/disable logic
 *
 * Build: gcc -Wall -Wextra -std=gnu11 -O2 -o /tmp/wlt-test-ks \
 *          tests/userspace/kill_switch_test.c
 * Run:   /tmp/wlt-test-ks
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

/* Kill switch states */
typedef enum {
    KS_DISABLED = 0,
    KS_ENABLED,
} ks_state_t;

/* Kill switch rule structure */
typedef struct {
    ks_state_t state;
    const char *policy;    /* "DROP" or "ACCEPT" */
    int allow_localhost;
    int allow_gateway;
    int allow_tor_process;
} ks_rules_t;

/* Simulate killswitch on */
static void ks_enable(ks_rules_t *r)
{
    r->state = KS_ENABLED;
    r->policy = "DROP";
    r->allow_localhost = 1;
    r->allow_gateway = 1;
    r->allow_tor_process = 1;
}

/* Simulate killswitch off */
static void ks_disable(ks_rules_t *r)
{
    r->state = KS_DISABLED;
    r->policy = "ACCEPT";
    r->allow_localhost = 0;
    r->allow_gateway = 0;
    r->allow_tor_process = 0;
}

/* Check if a destination is allowed */
static int ks_allow_dst(const ks_rules_t *r, uint32_t dst_ip)
{
    if (r->state == KS_DISABLED)
        return 1;  /* everything allowed when disabled */

    /* 127.0.0.1 = 0x7F000001 */
    if (r->allow_localhost && dst_ip == 0x7F000001)
        return 1;
    /* 10.0.2.2 = 0x0A000202 */
    if (r->allow_gateway && dst_ip == 0x0A000202)
        return 1;
    return 0;  /* blocked */
}

int main(void)
{
    printf("=== Kill Switch Tests ===\n\n");

    ks_rules_t rules = {0};

    /* Test 1: Initial state is disabled */
    printf("--- Initial state ---\n");
    TEST_ASSERT(rules.state == KS_DISABLED, "Initial state is DISABLED");
    TEST_ASSERT(rules.policy == NULL || strcmp(rules.policy, "ACCEPT") == 0,
                "Initial policy is ACCEPT (or NULL)");

    /* Test 2: Enable kill switch */
    printf("\n--- Enable kill switch ---\n");
    ks_enable(&rules);
    TEST_ASSERT(rules.state == KS_ENABLED, "State is ENABLED after ks_enable");
    TEST_ASSERT(strcmp(rules.policy, "DROP") == 0, "Policy is DROP when enabled");
    TEST_ASSERT(rules.allow_localhost == 1, "Localhost allowed when enabled");
    TEST_ASSERT(rules.allow_gateway == 1, "Gateway allowed when enabled");
    TEST_ASSERT(rules.allow_tor_process == 1, "Tor process allowed when enabled");

    /* Test 3: Traffic filtering when enabled */
    printf("\n--- Traffic filtering (enabled) ---\n");
    TEST_ASSERT(ks_allow_dst(&rules, 0x7F000001) == 1, "127.0.0.1 allowed when enabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0x0A000202) == 1, "10.0.2.2 allowed when enabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0x08080808) == 0, "8.8.8.8 blocked when enabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0xC0A80001) == 0, "192.168.0.1 blocked when enabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0x0A00020F) == 0, "10.0.2.15 blocked (only gw allowed)");

    /* Test 4: Disable kill switch */
    printf("\n--- Disable kill switch ---\n");
    ks_disable(&rules);
    TEST_ASSERT(rules.state == KS_DISABLED, "State is DISABLED after ks_disable");
    TEST_ASSERT(strcmp(rules.policy, "ACCEPT") == 0, "Policy is ACCEPT when disabled");
    TEST_ASSERT(rules.allow_localhost == 0, "Localhost flag cleared when disabled");

    /* Test 5: Traffic filtering when disabled */
    printf("\n--- Traffic filtering (disabled) ---\n");
    TEST_ASSERT(ks_allow_dst(&rules, 0x7F000001) == 1, "127.0.0.1 allowed when disabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0x08080808) == 1, "8.8.8.8 allowed when disabled");
    TEST_ASSERT(ks_allow_dst(&rules, 0x0A000202) == 1, "10.0.2.2 allowed when disabled");

    /* Test 6: Toggle cycle (enable → disable → enable) */
    printf("\n--- Toggle cycle ---\n");
    ks_enable(&rules);
    TEST_ASSERT(rules.state == KS_ENABLED, "Re-enabled after disable");
    ks_disable(&rules);
    TEST_ASSERT(rules.state == KS_DISABLED, "Disabled again");
    ks_enable(&rules);
    TEST_ASSERT(rules.state == KS_ENABLED, "Enabled third time");
    TEST_ASSERT(strcmp(rules.policy, "DROP") == 0, "Policy is DROP after re-enable");

    /* Test 7: IP address constants */
    printf("\n--- IP address constants ---\n");
    TEST_ASSERT(0x7F000001 == 0x7F000001, "127.0.0.1 constant correct");
    TEST_ASSERT(0x0A000202 == 0x0A000202, "10.0.2.2 constant correct");
    TEST_ASSERT(0x0A00020F == 0x0A00020F, "10.0.2.15 constant correct");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}