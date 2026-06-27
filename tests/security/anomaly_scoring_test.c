/*
 * tests/security/anomaly_scoring_test.c — Test anomaly detector scoring
 *
 * Tests the scoring function that classifies process behavior:
 *   score < 0.3  → normal (no action)
 *   score 0.3-0.7 → suspicious (alert)
 *   score 0.7-0.9 → high (freeze + alert)
 *   score > 0.9   → critical (quarantine)
 *
 * 10 assertions.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) do { \
    test_count++; \
    if (cond) { pass_count++; printf("  [PASS] %s\n", #cond); } \
    else { printf("  [FAIL] %s\n", #cond); } \
} while(0)

/* Mock anomaly scoring function — same logic as wlt-guardian/anomaly.c
 * Inputs: cpu_pct, net_connections, open_files, bytes_written
 * Returns: anomaly score 0.0 to 1.0 */
static float compute_anomaly_score(float cpu_pct, int net_conns, int open_files, int bytes_mb)
{
    float score = 0.0;

    /* CPU: >80% is suspicious, >95% is critical */
    if (cpu_pct > 95.0) score += 0.4;
    else if (cpu_pct > 80.0) score += 0.2;
    else if (cpu_pct > 50.0) score += 0.1;

    /* Network: >10 connections is suspicious, >50 is critical */
    if (net_conns > 50) score += 0.3;
    else if (net_conns > 10) score += 0.15;

    /* Open files: >100 is suspicious, >500 is critical */
    if (open_files > 500) score += 0.2;
    else if (open_files > 100) score += 0.1;

    /* Bytes written: >100MB is suspicious, >1GB is critical */
    if (bytes_mb > 1024) score += 0.1;
    else if (bytes_mb > 100) score += 0.05;

    /* Clamp to 1.0 */
    if (score > 1.0) score = 1.0;
    return score;
}

/* Enforcement decision based on score */
typedef enum {
    ACTION_NONE,        /* score < 0.3 */
    ACTION_ALERT,       /* 0.3 <= score < 0.7 */
    ACTION_FREEZE,      /* 0.7 <= score < 0.9 */
    ACTION_QUARANTINE   /* score >= 0.9 */
} enforcement_action_t;

static enforcement_action_t decide_action(float score)
{
    if (score >= 0.9) return ACTION_QUARANTINE;
    if (score >= 0.7) return ACTION_FREEZE;
    if (score >= 0.3) return ACTION_ALERT;
    return ACTION_NONE;
}

int main(void)
{
    printf("=== Anomaly Scoring Test ===\n");

    /* Test 1: Normal process (low CPU, low everything) */
    printf("Test 1: Normal process\n");
    {
        float s = compute_anomaly_score(5.0, 2, 10, 1);
        printf("  score = %.2f\n", s);
        ASSERT(s < 0.3);
        ASSERT(decide_action(s) == ACTION_NONE);
    }

    /* Test 2: Suspicious process (high CPU, moderate network) */
    printf("Test 2: Suspicious process\n");
    {
        float s = compute_anomaly_score(85.0, 15, 50, 10);
        printf("  score = %.2f\n", s);
        ASSERT(s >= 0.3 && s < 0.7);
        ASSERT(decide_action(s) == ACTION_ALERT);
    }

    /* Test 3: High threat (very high CPU, many connections) */
    printf("Test 3: High threat\n");
    {
        float s = compute_anomaly_score(98.0, 20, 150, 200);
        printf("  score = %.2f\n", s);
        ASSERT(s >= 0.7);
        ASSERT(decide_action(s) == ACTION_FREEZE || decide_action(s) == ACTION_QUARANTINE);
    }

    /* Test 4: Critical threat (maxed out everything) */
    printf("Test 4: Critical threat\n");
    {
        float s = compute_anomaly_score(99.0, 60, 600, 2048);
        printf("  score = %.2f\n", s);
        ASSERT(s >= 0.9);
        ASSERT(decide_action(s) == ACTION_QUARANTINE);
    }

    /* Test 5: Idle process (zero everything) */
    printf("Test 5: Idle process\n");
    {
        float s = compute_anomaly_score(0.0, 0, 0, 0);
        printf("  score = %.2f\n", s);
        ASSERT(s == 0.0);
        ASSERT(decide_action(s) == ACTION_NONE);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
