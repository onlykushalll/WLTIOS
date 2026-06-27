import sys
f = sys.argv[1]  # include/guardian.h
s = open(f).read()

# BUG 7 fix: add pcomm[16] to g_event_t so IOA rules can match parent comm.
old = """    char     comm[G_COMM_MAX];
    char     path[G_PATH_MAX];"""
new = """    char     comm[G_COMM_MAX];
    char     pcomm[G_COMM_MAX];   /* BUG 7: parent comm for IOA ancestry rules */
    char     path[G_PATH_MAX];"""
assert old in s, "g_event_t comm/path not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 7 part 1: pcomm added to g_event_t in guardian.h")
