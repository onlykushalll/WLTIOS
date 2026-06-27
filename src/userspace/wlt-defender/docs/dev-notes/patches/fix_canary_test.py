import sys
f = sys.argv[1]
s = open(f, encoding='utf-8').read()

old = '''        g_canary_engine_t *ce1 = g_canary_init();
        if (ce1 && ce1->n > 0) {
            g_canary_free(ce1);  /* canary files persist on disk */
            /* re-init \u2014 should read+hash existing canaries */
            g_canary_engine_t *ce2 = g_canary_init();
            if (ce2) {
                int n_bad = g_canary_verify_all(ce2);
                if (n_bad > 0) {
                    fprintf(stderr, "FAIL: canary restart produced %d false positive(s) \u2014 hash not loaded\\n", n_bad);
                    g_canary_free(ce2);
                    return 1;
                }
                printf("[test] Canary restart test passed (no false positives after re-init).\\n");
                g_canary_free(ce2);
            }
        } else {
            printf("[test] Skipping canary test (no writable canary dirs in this env).\\n");
        }'''

new = '''        g_canary_engine_t *ce1 = g_canary_init();
        if (ce1) {
            g_canary_free(ce1);  /* canary files persist on disk */
            /* re-init: should read+hash existing canaries */
            g_canary_engine_t *ce2 = g_canary_init();
            if (ce2) {
                int n_bad = g_canary_verify_all(ce2);
                if (n_bad > 0) {
                    fprintf(stderr, "FAIL: canary restart produced %d false positive(s) - hash not loaded\\n", n_bad);
                    g_canary_free(ce2);
                    return 1;
                }
                printf("[test] Canary restart test passed (no false positives after re-init).\\n");
                g_canary_free(ce2);
            }
        } else {
            printf("[test] Skipping canary test (no writable canary dirs in this env).\\n");
        }'''

assert old in s, "canary test block not found"
s = s.replace(old, new)
open(f, 'w', encoding='utf-8').write(s)
print("fixed: canary test no longer accesses opaque struct member")
