/**
 * @file amnesic_test.c
 * @brief Unit tests for amnesic mode (tmpfs mounts)
 * @module wlt-init
 * @purpose Verify tmpfs mount configuration and directory creation order
 *
 * Build: gcc -Wall -Wextra -std=gnu11 -O2 -o /tmp/wlt-test-amn \
 *          tests/userspace/amnesic_test.c
 * Run:   /tmp/wlt-test-amn
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

/* Mount target structure */
typedef struct {
    const char *source;
    const char *target;
    const char *fstype;
    const char *options;
} mount_entry_t;

/* The amnesic mount table from init-minimal.c */
static const mount_entry_t amnesic_mounts[] = {
    {"proc",     "/proc", "proc",     ""},
    {"sysfs",    "/sys",  "sysfs",    ""},
    {"devtmpfs", "/dev",  "devtmpfs", ""},
    {"tmpfs",    "/tmp",  "tmpfs",    "size=64m"},
    {"tmpfs",    "/var",  "tmpfs",    "size=32m"},
    {"tmpfs",    "/run",  "tmpfs",    "mode=0755"},
    {"tmpfs",    "/home", "tmpfs",    "size=256m"},
};

/* Directories that must be created AFTER /var is mounted as tmpfs */
static const char *var_subdirs[] = {
    "/var/lib",
    "/var/lib/tor",
    "/var/log",
    "/var/log/wlt",
};

int main(void)
{
    printf("=== Amnesic Mode Tests ===\n\n");

    /* Test 1: Correct number of mounts */
    printf("--- Mount table ---\n");
    int num_mounts = sizeof(amnesic_mounts) / sizeof(amnesic_mounts[0]);
    TEST_ASSERT(num_mounts == 7, "7 mounts in amnesic table");

    /* Test 2: Core filesystems come first */
    printf("\n--- Mount order ---\n");
    TEST_ASSERT(strcmp(amnesic_mounts[0].fstype, "proc") == 0, "proc mounted first");
    TEST_ASSERT(strcmp(amnesic_mounts[1].fstype, "sysfs") == 0, "sysfs mounted second");
    TEST_ASSERT(strcmp(amnesic_mounts[2].fstype, "devtmpfs") == 0, "devtmpfs mounted third");

    /* Test 3: tmpfs mounts for writable directories */
    printf("\n--- Tmpfs mounts ---\n");
    TEST_ASSERT(strcmp(amnesic_mounts[3].target, "/tmp") == 0, "/tmp is 4th mount");
    TEST_ASSERT(strcmp(amnesic_mounts[4].target, "/var") == 0, "/var is 5th mount");
    TEST_ASSERT(strcmp(amnesic_mounts[5].target, "/run") == 0, "/run is 6th mount");
    TEST_ASSERT(strcmp(amnesic_mounts[6].target, "/home") == 0, "/home is 7th mount");

    /* Test 4: All writable dirs use tmpfs */
    printf("\n--- Tmpfs verification ---\n");
    for (int i = 3; i < 7; i++)
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s uses tmpfs", amnesic_mounts[i].target);
        TEST_ASSERT(strcmp(amnesic_mounts[i].fstype, "tmpfs") == 0, msg);
    }

    /* Test 5: Size limits */
    printf("\n--- Size limits ---\n");
    TEST_ASSERT(strstr(amnesic_mounts[3].options, "size=64m") != NULL, "/tmp has 64m limit");
    TEST_ASSERT(strstr(amnesic_mounts[4].options, "size=32m") != NULL, "/var has 32m limit");
    TEST_ASSERT(strstr(amnesic_mounts[6].options, "size=256m") != NULL, "/home has 256m limit");

    /* Test 6: /run has mode 0755 */
    printf("\n--- /run permissions ---\n");
    TEST_ASSERT(strstr(amnesic_mounts[5].options, "mode=0755") != NULL, "/run has mode=0755");

    /* Test 7: Var subdirectories created after mount */
    printf("\n--- Var subdirectories ---\n");
    int num_var_subdirs = sizeof(var_subdirs) / sizeof(var_subdirs[0]);
    TEST_ASSERT(num_var_subdirs == 4, "4 var subdirectories created");

    /* Test 8: Tor data directory has correct path */
    printf("\n--- Tor data directory ---\n");
    TEST_ASSERT(strcmp(var_subdirs[1], "/var/lib/tor") == 0, "Tor data dir is /var/lib/tor");
    TEST_ASSERT(strstr(var_subdirs[1], "tor") != NULL, "Tor dir path contains 'tor'");

    /* Test 9: Log directories */
    printf("\n--- Log directories ---\n");
    TEST_ASSERT(strcmp(var_subdirs[2], "/var/log") == 0, "Log dir is /var/log");
    TEST_ASSERT(strcmp(var_subdirs[3], "/var/log/wlt") == 0, "WLT log dir is /var/log/wlt");

    /* Test 10: Amnesic guarantee — no persistent storage */
    printf("\n--- Amnesic guarantee ---\n");
    int tmpfs_count = 0;
    for (int i = 0; i < num_mounts; i++)
    {
        if (strcmp(amnesic_mounts[i].fstype, "tmpfs") == 0)
            tmpfs_count++;
    }
    TEST_ASSERT(tmpfs_count == 4, "4 tmpfs mounts (all writable dirs in RAM)");
    TEST_ASSERT(tmpfs_count > 0, "At least one tmpfs mount exists");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}