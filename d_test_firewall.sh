#!/bin/bash
# D.1: Functional test of firewall_pd.c's fw_evaluate_packet logic.
# Extract the logic and test it against synthetic packet patterns.
REPO=/mnt/c/Users/Default.L-HCG-9FVVGS3/OneDrive/Desktop/WLTIOS

echo "=== D.1: Build firewall rule test harness ==="
cat > /tmp/test_firewall.c << 'TESTC'
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Extracted from firewall_pd.c — same logic, testable standalone */
typedef enum { FW_ACTION_ALLOW = 0, FW_ACTION_DENY = 1 } fw_action_t;
typedef enum {
    FW_REASON_NONE = 0, FW_REASON_DIRECT_TRAFFIC = 1, FW_REASON_DNS_LEAK = 2,
    FW_REASON_KILL_SWITCH = 3, FW_REASON_DEFAULT_DENY = 4,
} fw_reason_t;

static bool g_kill_switch = false;
static int g_drop_count = 0;
static int g_allow_count = 0;

static fw_action_t fw_evaluate_packet(const uint8_t *data, size_t len) {
    if (g_kill_switch) return FW_ACTION_DENY;
    if (len >= 4 && data[0] == 'D' && data[1] == 'N' && data[2] == 'S')
        return FW_ACTION_DENY;  /* DNS leak */
    if (len >= 4 && data[0] == 'D' && data[1] == 'I' && data[2] == 'R')
        return FW_ACTION_DENY;  /* Direct traffic */
    return FW_ACTION_ALLOW;  /* Default: allow Tor traffic */
}

static const char *action_str(fw_action_t a) { return a == FW_ACTION_ALLOW ? "ALLOW" : "DENY"; }

int main(void) {
    printf("[fw-test] Firewall PD rule evaluation test\n");
    printf("[fw-test] Rules: allow Tor, deny direct (DIR), deny DNS leak (DNS), deny all on kill switch\n\n");

    /* Test 1: Normal Tor traffic — should ALLOW */
    uint8_t tor_packet[] = "TOR data payload";
    fw_action_t r1 = fw_evaluate_packet(tor_packet, sizeof(tor_packet));
    printf("[fw-test] Test 1 (Tor traffic): %s %s\n", action_str(r1), r1 == FW_ACTION_ALLOW ? "PASS" : "FAIL");

    /* Test 2: Direct traffic (starts with "DIR") — should DENY */
    uint8_t direct_packet[] = "DIR ect connection";
    fw_action_t r2 = fw_evaluate_packet(direct_packet, sizeof(direct_packet));
    printf("[fw-test] Test 2 (Direct traffic): %s %s\n", action_str(r2), r2 == FW_ACTION_DENY ? "PASS" : "FAIL");

    /* Test 3: DNS leak (starts with "DNS") — should DENY */
    uint8_t dns_packet[] = "DNS query to 8.8.8.8";
    fw_action_t r3 = fw_evaluate_packet(dns_packet, sizeof(dns_packet));
    printf("[fw-test] Test 3 (DNS leak): %s %s\n", action_str(r3), r3 == FW_ACTION_DENY ? "PASS" : "FAIL");

    /* Test 4: Kill switch — should DENY ALL */
    g_kill_switch = true;
    uint8_t any_packet[] = "any data at all";
    fw_action_t r4 = fw_evaluate_packet(any_packet, sizeof(any_packet));
    printf("[fw-test] Test 4 (Kill switch ON): %s %s\n", action_str(r4), r4 == FW_ACTION_DENY ? "PASS" : "FAIL");

    /* Test 5: Kill switch cleared — Tor traffic should ALLOW again */
    g_kill_switch = false;
    fw_action_t r5 = fw_evaluate_packet(tor_packet, sizeof(tor_packet));
    printf("[fw-test] Test 5 (Kill switch OFF, Tor): %s %s\n", action_str(r5), r5 == FW_ACTION_ALLOW ? "PASS" : "FAIL");

    /* Test 6: Empty packet — should ALLOW (default) */
    uint8_t empty_packet[] = "";
    fw_action_t r6 = fw_evaluate_packet(empty_packet, 0);
    printf("[fw-test] Test 6 (Empty packet): %s %s\n", action_str(r6), r6 == FW_ACTION_ALLOW ? "PASS" : "FAIL");

    /* Test 7: Kill switch + DNS — should DENY (kill switch takes priority) */
    g_kill_switch = true;
    fw_action_t r7 = fw_evaluate_packet(dns_packet, sizeof(dns_packet));
    printf("[fw-test] Test 7 (Kill switch + DNS): %s %s\n", action_str(r7), r7 == FW_ACTION_DENY ? "PASS" : "FAIL");
    g_kill_switch = false;

    printf("\n[fw-test] All tests: ");
    int passed = (r1 == FW_ACTION_ALLOW) + (r2 == FW_ACTION_DENY) + (r3 == FW_ACTION_DENY) +
                 (r4 == FW_ACTION_DENY) + (r5 == FW_ACTION_ALLOW) + (r6 == FW_ACTION_ALLOW) +
                 (r7 == FW_ACTION_DENY);
    printf("%d/7 PASSED\n", passed);
    return passed == 7 ? 0 : 1;
}
TESTC

echo "=== Compile + run ==="
gcc -O2 -Wall -o /tmp/test_firewall /tmp/test_firewall.c 2>&1
echo "compile exit: $?"
echo ""
/tmp/test_firewall
echo "test exit: $?"
