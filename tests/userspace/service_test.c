/**
 * @file service_test.c
 * @brief Unit tests for WLT-Init service parser
 * @module wlt-init/service
 * @purpose Test INI parsing of .service files
 * @spec: docs/specs/wlt-init.md
 *
 * Build: gcc -o service_test service_test.c ../../src/userspace/wlt-init/service.c ../../src/userspace/wlt-init/dependency.c ../../src/userspace/wlt-init/log.c -I../../src/userspace/wlt-init/include
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../src/userspace/wlt-init/include/init.h"
#include "../../src/userspace/wlt-init/include/service.h"

/* Stub: wlt_init_error_str is in init.c (which has main(), so we stub it) */
const char *wlt_init_error_str(wlt_init_error_t err) {
    switch (err) {
        case WLT_INIT_OK: return "OK";
        case WLT_INIT_ERR_NOT_FOUND: return "Not found";
        case WLT_INIT_ERR_PARSE: return "Parse error";
        default: return "Error";
    }
}

/* Test: Parse a service file */
static const char *test_service =
    "[wlt-service]\n"
    "name = Test Service\n"
    "exec = /usr/bin/test-app\n"
    "args = --verbose --port 8080\n"
    "user = wlt\n"
    "\n"
    "[config]\n"
    "depends = tor-vm,key-vault\n"
    "tor_required = yes\n"
    "restart_on_crash = yes\n"
    "restart_backoff_ms = 3000\n"
    "priority = critical\n";

int main(void)
{
    int pass = 0, fail = 0;
    wlt_service_t svc;

    printf("=== WLT-Init Service Parser Test ===\n\n");

    /* Parse the test service content directly */
    /* TODO(wlt-init): Add wlt_service_parse_string() for testing */
    /* For now, write to temp file + parse */
    FILE *f = fopen("/tmp/test.service", "w");
    if (f == NULL)
    {
        printf("[FAIL] Cannot create test file\n");
        return 1;
    }
    fputs(test_service, f);
    fclose(f);

    wlt_init_error_t err = wlt_service_load("/tmp/test.service", &svc);
    if (err != WLT_INIT_OK)
    {
        printf("[FAIL] Failed to parse service: %s\n", wlt_init_error_str(err));
        fail++;
        printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
        return 1;
    }

    /* Test 1: Name */
    if (strcmp(svc.name, "Test Service") == 0)
    {
        printf("[PASS] Test 1: name = %s\n", svc.name);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 1: Expected 'Test Service', got '%s'\n", svc.name);
        fail++;
    }

    /* Test 2: Exec path */
    if (strcmp(svc.exec, "/usr/bin/test-app") == 0)
    {
        printf("[PASS] Test 2: exec = %s\n", svc.exec);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 2: Expected '/usr/bin/test-app', got '%s'\n", svc.exec);
        fail++;
    }

    /* Test 3: Args */
    if (strcmp(svc.args, "--verbose --port 8080") == 0)
    {
        printf("[PASS] Test 3: args = %s\n", svc.args);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 3: Expected '--verbose --port 8080', got '%s'\n", svc.args);
        fail++;
    }

    /* Test 4: User */
    if (strcmp(svc.user, "wlt") == 0)
    {
        printf("[PASS] Test 4: user = %s\n", svc.user);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 4: Expected 'wlt', got '%s'\n", svc.user);
        fail++;
    }

    /* Test 5: Dependencies */
    if (svc.depend_count == 2 &&
        strcmp(svc.depends[0], "tor-vm") == 0 &&
        strcmp(svc.depends[1], "key-vault") == 0)
    {
        printf("[PASS] Test 5: depends = %s, %s\n", svc.depends[0], svc.depends[1]);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 5: Expected 2 deps (tor-vm, key-vault), got %zu\n",
               svc.depend_count);
        fail++;
    }

    /* Test 6: Tor required */
    if (svc.tor_required == true)
    {
        printf("[PASS] Test 6: tor_required = yes\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 6: Expected tor_required = true\n");
        fail++;
    }

    /* Test 7: Restart on crash */
    if (svc.restart_on_crash == true)
    {
        printf("[PASS] Test 7: restart_on_crash = yes\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 7: Expected restart_on_crash = true\n");
        fail++;
    }

    /* Test 8: Restart backoff */
    if (svc.restart_backoff_ms == 3000)
    {
        printf("[PASS] Test 8: restart_backoff_ms = %u\n", svc.restart_backoff_ms);
        pass++;
    }
    else
    {
        printf("[FAIL] Test 8: Expected 3000, got %u\n", svc.restart_backoff_ms);
        fail++;
    }

    /* Test 9: Priority */
    if (svc.priority == WLT_PRIO_CRITICAL)
    {
        printf("[PASS] Test 9: priority = critical\n");
        pass++;
    }
    else
    {
        printf("[FAIL] Test 9: Expected critical (%d), got %d\n",
               WLT_PRIO_CRITICAL, svc.priority);
        fail++;
    }

    /* Cleanup */
    unlink("/tmp/test.service");

    printf("\n=== Summary: %d pass, %d fail ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
