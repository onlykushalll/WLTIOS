#!/usr/bin/env python3
"""Count braces in g_daemon_self_test and find the missing brace."""
import sys
f = sys.argv[1]
s = open(f).read()
start = s.find('int g_daemon_self_test')
end = s.find('static int g_sig_scan_all_proc_mem')
body = s[start:end]
o = body.count('{')
c = body.count('}')
print(f"g_daemon_self_test: open={o} close={c} diff={o-c}")
if o != c:
    # find the line where the imbalance occurs — walk through tracking depth
    lines = body.split('\n')
    depth = 0
    for i, line in enumerate(lines):
        depth += line.count('{') - line.count('}')
        if depth < 0:
            print(f"  depth went negative at line {i+1}: {line.strip()[:80]}")
            break
    print(f"  final depth: {depth} (should be 0)")
    # show the iforest test block region (where tighten2 edited)
    for i, line in enumerate(lines):
        if 'TIGHTER assertions' in line or 'iforest struct alignment' in line:
            print(f"  around line {i+1}: {line.strip()[:80]}")
