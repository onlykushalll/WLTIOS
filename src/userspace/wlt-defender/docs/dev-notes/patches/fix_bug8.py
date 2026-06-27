import sys
f = sys.argv[1]  # src/fim_engine.c
s = open(f).read()

# BUG 8 fix: g_fim_restore still returns -ENOSYS while g_action_restore_file
# (action_engine.c) does the real work. Two divergent restore paths. Delegate.
old = """int g_fim_restore(g_fim_engine_t *fe, const char *path) {
    /* baseline stores hash only — for true restore we need a baseline copy
     * of the file. v1: log that restore is needed but can't perform.
     * v2: store baseline copies in /etc/wlt/guardian/baseline/ at build time. */
    (void)fe;
    printf("[fim] restore requested for %s — need baseline copy (v2 feature)\\n", path);
    return -ENOSYS;
}"""
new = """int g_fim_restore(g_fim_engine_t *fe, const char *path) {
    /* BUG 8 fix: delegate to g_action_restore_file (Phase 1.3) which checks
     * the baseline directory first, then quarantine. Old code returned -ENOSYS
     * (stub) while action_engine.c had the real implementation — two divergent
     * restore paths. Now there's one authority. */
    (void)fe;
    return g_action_restore_file(path);
}"""
assert old in s, "g_fim_restore stub not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 8 fixed: g_fim_restore delegates to g_action_restore_file")
