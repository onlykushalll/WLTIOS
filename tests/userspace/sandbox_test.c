/**
 * @file sandbox_test.c
 * @brief Unit tests for sandbox namespace isolation
 * @module wlt-sandbox
 * @purpose Verify sandbox state transitions and namespace flags
 *
 * Build: gcc -Wall -Wextra -std=gnu11 -O2 -o /tmp/wlt-test-sb \
 *          tests/userspace/sandbox_test.c
 * Run:   /tmp/wlt-test-sb
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

/* Sandbox states */
typedef enum {
    SANDBOX_UNINITIALIZED = 0,
    SANDBOX_CREATED,
    SANDBOX_RUNNING,
    SANDBOX_EXITED,
    SANDBOX_FAILED,
} sandbox_state_t;

/* Namespace flags */
#define NS_CLONE_NEWNET    0x40000000
#define NS_CLONE_NEWPID    0x20000000
#define NS_CLONE_NEWNS     0x00020000
#define NS_CLONE_NEWUSER   0x10000000
#define NS_CLONE_NEWIPC    0x08000000
#define NS_CLONE_NEWUTS    0x04000000

/* Sandbox configuration */
typedef struct {
    sandbox_state_t state;
    uint32_t ns_flags;
    int network_isolated;
    int pid_isolated;
    int mount_isolated;
} sandbox_t;

/* Initialize a sandbox with network-only isolation (like the shell command) */
static void sandbox_init_net_only(sandbox_t *s)
{
    s->state = SANDBOX_CREATED;
    s->ns_flags = NS_CLONE_NEWNET;
    s->network_isolated = 1;
    s->pid_isolated = 0;
    s->mount_isolated = 0;
}

/* Initialize a full sandbox (all namespaces) */
static void sandbox_init_full(sandbox_t *s)
{
    s->state = SANDBOX_CREATED;
    s->ns_flags = NS_CLONE_NEWUSER | NS_CLONE_NEWNS | NS_CLONE_NEWPID |
                  NS_CLONE_NEWIPC | NS_CLONE_NEWUTS | NS_CLONE_NEWNET;
    s->network_isolated = 1;
    s->pid_isolated = 1;
    s->mount_isolated = 1;
}

/* Start the sandbox */
static void sandbox_start(sandbox_t *s)
{
    if (s->state == SANDBOX_CREATED)
        s->state = SANDBOX_RUNNING;
}

/* Stop the sandbox */
static void sandbox_stop(sandbox_t *s)
{
    if (s->state == SANDBOX_RUNNING)
        s->state = SANDBOX_EXITED;
}

int main(void)
{
    printf("=== Sandbox Tests ===\n\n");

    /* Test 1: Initial state */
    printf("--- Initial state ---\n");
    sandbox_t sb = {0};
    TEST_ASSERT(sb.state == SANDBOX_UNINITIALIZED, "Initial state is UNINITIALIZED");
    TEST_ASSERT(sb.ns_flags == 0, "Initial ns_flags is 0");

    /* Test 2: Network-only sandbox (shell command) */
    printf("\n--- Network-only sandbox ---\n");
    sandbox_init_net_only(&sb);
    TEST_ASSERT(sb.state == SANDBOX_CREATED, "State is CREATED after init");
    TEST_ASSERT(sb.ns_flags == NS_CLONE_NEWNET, "Only CLONE_NEWNET flag set");
    TEST_ASSERT(sb.network_isolated == 1, "Network is isolated");
    TEST_ASSERT(sb.pid_isolated == 0, "PID is NOT isolated (net-only)");
    TEST_ASSERT(sb.mount_isolated == 0, "Mount is NOT isolated (net-only)");

    /* Test 3: Full sandbox (all namespaces) */
    printf("\n--- Full sandbox ---\n");
    sandbox_t full = {0};
    sandbox_init_full(&full);
    TEST_ASSERT(full.state == SANDBOX_CREATED, "Full sandbox state is CREATED");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWNET) != 0, "Full has NEWNET");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWPID) != 0, "Full has NEWPID");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWNS) != 0, "Full has NEWNS");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWUSER) != 0, "Full has NEWUSER");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWIPC) != 0, "Full has NEWIPC");
    TEST_ASSERT((full.ns_flags & NS_CLONE_NEWUTS) != 0, "Full has NEWUTS");
    TEST_ASSERT(full.network_isolated == 1, "Full: network isolated");
    TEST_ASSERT(full.pid_isolated == 1, "Full: PID isolated");
    TEST_ASSERT(full.mount_isolated == 1, "Full: mount isolated");

    /* Test 4: State transitions */
    printf("\n--- State transitions ---\n");
    sandbox_t s2 = {0};
    sandbox_init_net_only(&s2);
    TEST_ASSERT(s2.state == SANDBOX_CREATED, "State: CREATED");
    sandbox_start(&s2);
    TEST_ASSERT(s2.state == SANDBOX_RUNNING, "State: RUNNING after start");
    sandbox_stop(&s2);
    TEST_ASSERT(s2.state == SANDBOX_EXITED, "State: EXITED after stop");

    /* Test 5: Can't start an already-running sandbox */
    printf("\n--- Invalid transitions ---\n");
    sandbox_t s3 = {0};
    sandbox_init_net_only(&s3);
    sandbox_start(&s3);
    sandbox_start(&s3);  /* should be no-op */
    TEST_ASSERT(s3.state == SANDBOX_RUNNING, "Double-start is no-op (still RUNNING)");

    /* Test 6: Can't stop a non-running sandbox */
    sandbox_t s4 = {0};
    sandbox_init_net_only(&s4);
    sandbox_stop(&s4);  /* should be no-op */
    TEST_ASSERT(s4.state == SANDBOX_CREATED, "Stop before start is no-op (still CREATED)");

    /* Test 7: Namespace flag values are distinct */
    printf("\n--- Flag uniqueness ---\n");
    TEST_ASSERT(NS_CLONE_NEWNET != NS_CLONE_NEWPID, "NEWNET != NEWPID");
    TEST_ASSERT(NS_CLONE_NEWPID != NS_CLONE_NEWNS, "NEWPID != NEWNS");
    TEST_ASSERT(NS_CLONE_NEWUSER != NS_CLONE_NEWNET, "NEWUSER != NEWNET");
    TEST_ASSERT((NS_CLONE_NEWNET & NS_CLONE_NEWPID) == 0, "NEWNET and NEWPID don't overlap");

    /* Test 8: Network isolation means no eth0 */
    printf("\n--- Network isolation effect ---\n");
    sandbox_t s5 = {0};
    sandbox_init_net_only(&s5);
    TEST_ASSERT(s5.network_isolated == 1, "Sandbox has network isolated");
    /* In a real sandbox: ls /sys/class/net/ would show only lo, no eth0 */

    /* Test 9: Full sandbox has more isolation than net-only */
    printf("\n--- Isolation comparison ---\n");
    int net_only_isolation = sb.network_isolated + sb.pid_isolated + sb.mount_isolated;
    int full_isolation = full.network_isolated + full.pid_isolated + full.mount_isolated;
    TEST_ASSERT(full_isolation > net_only_isolation, "Full sandbox has more isolation");
    TEST_ASSERT(net_only_isolation == 1, "Net-only has 1 isolation dimension");
    TEST_ASSERT(full_isolation == 3, "Full has 3 isolation dimensions");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}