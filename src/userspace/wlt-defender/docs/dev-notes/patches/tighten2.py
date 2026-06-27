#!/usr/bin/env python3
import re, sys
f = sys.argv[1]
s = open(f, encoding='utf-8').read()
pattern = r'/\* the key assertion: anomalous must score higher than normal\.[\s\S]*?if \(s_anom <= s_normal\) \{[\s\S]*?\}\s*\}'
m = re.search(pattern, s)
if not m:
    print("PATTERN NOT FOUND")
    sys.exit(1)
old_block = m.group(0)
new_block = """/* TIGHTER assertions (catch struct-alignment garbage):
             * 1. anomalous must score higher than normal (ordering)
             * 2. normal must be < 0.55 (a normal process should score low)
             * 3. anomalous must be > 0.55 (an anomalous process should score high)
             * 4. the gap must be > 0.1 (meaningful separation, not noise)
             * Misaligned struct reads produce scores clustered near 0.5 (garbage),
             * which fails assertions 2+3+4 even if assertion 1 accidentally passes. */
            if (s_anom <= s_normal) {
                fprintf(stderr, "FAIL: anomalous score (%.4f) <= normal score (%.4f) - iforest struct alignment bug\\n", s_anom, s_normal);
                g_iforest_free(ifo);
                return 1;
            }
            if (s_normal > 0.55) {
                fprintf(stderr, "FAIL: normal score %.4f too high (>0.55) - iforest struct alignment bug (garbage scores)\\n", s_normal);
                g_iforest_free(ifo);
                return 1;
            }
            if (s_anom < 0.55) {
                fprintf(stderr, "FAIL: anomalous score %.4f too low (<0.55) - iforest struct alignment bug (garbage scores)\\n", s_anom);
                g_iforest_free(ifo);
                return 1;
            }
            if (s_anom - s_normal < 0.1) {
                fprintf(stderr, "FAIL: score gap too small (%.4f - %.4f = %.4f < 0.1) - iforest struct alignment bug\\n", s_anom, s_normal, s_anom - s_normal);
                g_iforest_free(ifo);
                return 1;
            }"""
s = s.replace(old_block, new_block)
open(f, 'w', encoding='utf-8').write(s)
print("tightened iforest test assertions (4 checks)")
