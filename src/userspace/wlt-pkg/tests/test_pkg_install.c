/*
 * test_pkg_install.c — Section D: real install/remove cycle test
 *
 * Creates a real .tar.gz package, installs it via wlt_pkg_install,
 * verifies the binary is symlinked in /usr/bin and runs, then
 * removes it and verifies it's gone.
 *
 * Build: gcc -O2 -Wall -I../wlt-pkg/include test_pkg_install.c \
 *         -L../wlt-pkg -lwlt-pkg -o test_pkg_install
 */
#define _GNU_SOURCE
#include "pkg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PKG_NAME "test-hello"
#define BIN_NAME "test-hello"

/* Bin dir override (set via env var WLT_PKG_BIN_DIR) */
static const char *get_bin_dir(void) {
    const char *e = getenv("WLT_PKG_BIN_DIR");
    return e ? e : "/usr/bin";
}

static int create_test_package(void)
{
    /* Create cache dir (use env var if set, else default) */
    const char *cache = getenv("WLT_PKG_CACHE_DIR");
    if (!cache) cache = "/var/wlt/cache";
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", cache);
    int unused_mkdir = system(cmd); (void)unused_mkdir;

    /* Create a temp dir with the package structure */
    mkdir("/tmp/pkg-build", 0755);
    mkdir("/tmp/pkg-build/usr", 0755);
    mkdir("/tmp/pkg-build/usr/bin", 0755);

    /* Write a simple shell script as the "binary" */
    FILE *f = fopen("/tmp/pkg-build/usr/bin/" BIN_NAME, "w");
    if (!f) { perror("create binary"); return -1; }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "echo 'Hello from test-hello package!'\n");
    fclose(f);
    chmod("/tmp/pkg-build/usr/bin/" BIN_NAME, 0755);

    /* Create the tarball in the cache dir */
    char tar_cmd[512];
    snprintf(tar_cmd, sizeof(tar_cmd),
             "cd /tmp/pkg-build && tar czf %s/%s.tar.gz usr 2>/dev/null",
             cache, PKG_NAME);
    int rc = system(tar_cmd);
    if (rc != 0) { fprintf(stderr, "tar failed\n"); return -1; }

    /* Clean up temp dir */
    int unused_rc = system("rm -rf /tmp/pkg-build"); (void)unused_rc;

    printf("[test] package created: /var/wlt/cache/%s.tar.gz\n", PKG_NAME);
    return 0;
}

static int run_binary(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s/" BIN_NAME " 2>&1", get_bin_dir());
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    char buf[256] = {0};
    if (fgets(buf, sizeof(buf), p) == NULL) buf[0] = '\0';
    pclose(p);
    printf("[test] binary output: %s", buf);
    return (strstr(buf, "Hello from test-hello") != NULL) ? 0 : -1;
}

int main(void)
{
    int failures = 0;

    printf("=== Section D: wlt-pkg real install/remove cycle ===\n\n");

    /* Step 1: Init */
    printf("--- Step 1: wlt_pkg_init ---\n");
    wlt_pkg_error_t e = wlt_pkg_init();
    if (e != WLT_PKG_OK) {
        printf("FAIL: init: %s\n", wlt_pkg_error_str(e));
        return 1;
    }
    printf("OK\n\n");

    /* Step 2: Create test package */
    printf("--- Step 2: Create test package ---\n");
    if (create_test_package() != 0) {
        printf("FAIL: cannot create package\n");
        return 1;
    }
    printf("OK\n\n");

    /* Step 3: Install */
    printf("--- Step 3: wlt_pkg_install(\"%s\") ---\n", PKG_NAME);
    e = wlt_pkg_install(PKG_NAME);
    if (e != WLT_PKG_OK) {
        printf("FAIL: install: %s\n", wlt_pkg_error_str(e));
        return 1;
    }
    printf("OK\n\n");

    /* Step 4: Verify binary exists and runs */
    printf("--- Step 4: Verify binary runs ---\n");
    char bin_path[512];
    snprintf(bin_path, sizeof(bin_path), "%s/%s", get_bin_dir(), BIN_NAME);
    if (access(bin_path, X_OK) != 0) {
        printf("FAIL: %s not executable\n", bin_path);
        failures++;
    } else if (run_binary() != 0) {
        printf("FAIL: binary didn't produce expected output\n");
        failures++;
    } else {
        printf("OK\n");
    }
    printf("\n");

    /* Step 5: Verify it's in the database */
    printf("--- Step 5: Verify database ---\n");
    wlt_pkg_database_t db;
    e = wlt_pkg_list(&db);
    if (e != WLT_PKG_OK) {
        printf("FAIL: list: %s\n", wlt_pkg_error_str(e));
        failures++;
    } else {
        bool found = false;
        for (size_t i = 0; i < db.count; i++) {
            if (strcmp(db.packages[i].name, PKG_NAME) == 0 && db.packages[i].installed) {
                found = true;
                printf("  found: %s v%s (hash: %s)\n",
                       db.packages[i].name, db.packages[i].version,
                       db.packages[i].store_hash);
                break;
            }
        }
        if (!found) {
            printf("FAIL: %s not in database\n", PKG_NAME);
            failures++;
        } else {
            printf("OK\n");
        }
    }
    printf("\n");

    /* Step 6: Verify dependency.c uses real db */
    printf("--- Step 6: Verify dependency tracking ---\n");
    bool installed = wlt_pkg_is_installed(PKG_NAME);
    if (!installed) {
        printf("FAIL: wlt_pkg_is_installed returned false\n");
        failures++;
    } else {
        printf("OK: wlt_pkg_is_installed returned true\n");
    }
    printf("\n");

    /* Step 7: Remove */
    printf("--- Step 7: wlt_pkg_remove(\"%s\") ---\n", PKG_NAME);
    e = wlt_pkg_remove(PKG_NAME);
    if (e != WLT_PKG_OK) {
        printf("FAIL: remove: %s\n", wlt_pkg_error_str(e));
        failures++;
    } else {
        printf("OK\n");
    }
    printf("\n");

    /* Step 8: Verify binary is gone */
    printf("--- Step 8: Verify binary removed ---\n");
    if (access(bin_path, F_OK) == 0) {
        printf("FAIL: %s still exists\n", bin_path);
        failures++;
    } else {
        printf("OK: binary removed\n");
    }
    printf("\n");

    /* Step 9: Verify it's marked uninstalled in db */
    printf("--- Step 9: Verify database updated ---\n");
    e = wlt_pkg_list(&db);
    if (e == WLT_PKG_OK) {
        bool still_installed = false;
        for (size_t i = 0; i < db.count; i++) {
            if (strcmp(db.packages[i].name, PKG_NAME) == 0 && db.packages[i].installed) {
                still_installed = true;
                break;
            }
        }
        if (still_installed) {
            printf("FAIL: %s still marked installed\n", PKG_NAME);
            failures++;
        } else {
            printf("OK: marked uninstalled\n");
        }
    }
    printf("\n");

    /* Cleanup */
    {
        char cleanup_path[512];
        snprintf(cleanup_path, sizeof(cleanup_path), "%s/%s.tar.gz",
                 getenv("WLT_PKG_CACHE_DIR") ? getenv("WLT_PKG_CACHE_DIR") : "/var/wlt/cache",
                 PKG_NAME);
        unlink(cleanup_path);
    }

    printf("=== Results: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
