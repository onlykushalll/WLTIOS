/*
 * test_perm_deny.c — Section B permission denial test
 *
 * Creates a test app manifest requesting network access, creates a
 * grant file with network=DENY, then calls wlt_sandbox_launch and
 * verifies it returns WLT_SANDBOX_ERR_PERMISSION.
 *
 * Uses /tmp for grant files so no root needed.
 */
#define _GNU_SOURCE
#include "sandbox.h"
#include "permissions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MANIFEST_PATH "/tmp/test-net-app.manifest"
#define GRANTS_DIR    "/tmp/wlt-test-grants"
#define GRANT_FILE    GRANTS_DIR "/test-net-app.ini"

static int write_manifest(void)
{
    FILE *f = fopen(MANIFEST_PATH, "w");
    if (!f) { perror("manifest"); return -1; }
    fprintf(f, "[app]\n");
    fprintf(f, "name = test-net-app\n");
    fprintf(f, "exec = /bin/true\n");
    fprintf(f, "args = \n");
    fprintf(f, "[permissions]\n");
    fprintf(f, "network = tor-only\n");
    fprintf(f, "filesystem = none\n");
    fprintf(f, "[seccomp]\n");
    fprintf(f, "allowed = basic\n");
    fclose(f);
    printf("[test] manifest written: %s\n", MANIFEST_PATH);
    return 0;
}

static int write_grant(const char *network_value)
{
    mkdir(GRANTS_DIR, 0755);
    FILE *f = fopen(GRANT_FILE, "w");
    if (!f) { perror("grant"); return -1; }
    fprintf(f, "[meta]\n");
    fprintf(f, "app = test-net-app\n");
    fprintf(f, "updated_by = test\n");
    fprintf(f, "[grants]\n");
    fprintf(f, "network = %s\n", network_value);
    fclose(f);
    printf("[test] grant written: %s (network=%s)\n", GRANT_FILE, network_value);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "deny";
    int expect_perm_denied = (strcmp(mode, "deny") == 0);

    printf("=== Section B: Permission %s test ===\n", mode);

    if (write_manifest() != 0) return 1;
    if (write_grant(mode) != 0) return 1;

    /* Init sandbox manager — permission check fires BEFORE cgroup setup,
     * so it works even without root/cgroup access */
    wlt_SandboxManager mgr;
    memset(&mgr, 0, sizeof(mgr));
    wlt_sandbox_error_t e = wlt_sandbox_init(&mgr, "/tmp/wlt-test-cgroup");
    if (e != WLT_SANDBOX_OK) {
        fprintf(stderr, "[test] sandbox_init: %s (non-fatal, perm check fires first)\n",
                wlt_sandbox_error_str(e));
    }

    /* Debug: verify the permission manager can find the grant */
    {
        wlt_PermsManager dbg_perms;
        if (wlt_perms_init(&dbg_perms, GRANTS_DIR) == WLT_PERMS_OK) {
            wlt_perms_error_t le = wlt_perms_load(&dbg_perms, "test-net-app");
            printf("[test] debug: wlt_perms_load returned %d\n", le);
            wlt_PermsApp *papp = wlt_perms_find(&dbg_perms, "test-net-app");
            printf("[test] debug: wlt_perms_find returned %p\n", (void*)papp);
            if (papp) {
                wlt_perms_decision_t d = wlt_perms_check(&dbg_perms, "test-net-app", WLT_PERM_NETWORK);
                printf("[test] debug: wlt_perms_check returned %d\n", d);
            }
        } else {
            printf("[test] debug: wlt_perms_init FAILED\n");
        }
    }

    /* Launch the app — triggers the permission check in launch.c */
    printf("[test] launching %s ...\n", MANIFEST_PATH);
    wlt_SandboxApp *app = NULL;
    e = wlt_sandbox_launch(&mgr, MANIFEST_PATH, "root", &app);

    printf("[test] launch returned: %d (%s)\n", e, wlt_sandbox_error_str(e));

    if (expect_perm_denied) {
        if (e == WLT_SANDBOX_ERR_PERMISSION) {
            printf("[test] PASS: permission DENIED as expected\n");
            return 0;
        } else {
            printf("[test] FAIL: expected PERMISSION (-5), got %d\n", e);
            return 1;
        }
    } else {
        if (e == WLT_SANDBOX_ERR_PERMISSION) {
            printf("[test] FAIL: permission denied but grant was allow\n");
            return 1;
        } else {
            printf("[test] PASS: NOT permission-denied (got %d — cgroup/ns error OK)\n", e);
            return 0;
        }
    }
}
