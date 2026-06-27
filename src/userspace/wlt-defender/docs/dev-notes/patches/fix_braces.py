#!/usr/bin/env python3
"""Fix the 2 missing closing braces after the iforest test block."""
import sys
f = sys.argv[1]
s = open(f).read()

# The iforest test's last if block ends with "}", then should have 2 more "}" to close
# the "if (ifo) {" block and the test-13 outer block, before "/* 14. Ransomware"
old = '''                fprintf(stderr, "FAIL: score gap too small (%.4f - %.4f = %.4f < 0.1) - iforest struct alignment bug\\n", s_anom, s_normal, s_anom - s_normal);
                g_iforest_free(ifo);
                return 1;
            }

    /* 14. Ransomware rate-based detection test (BUG 4 regression catch).'''

new = '''                fprintf(stderr, "FAIL: score gap too small (%.4f - %.4f = %.4f < 0.1) - iforest struct alignment bug\\n", s_anom, s_normal, s_anom - s_normal);
                g_iforest_free(ifo);
                return 1;
            }
            printf("[test] Isolation Forest inference correctness passed (anomalous > normal).\\n");
            g_iforest_free(ifo);
        }
    }

    /* 14. Ransomware rate-based detection test (BUG 4 regression catch).'''

if old in s:
    s = s.replace(old, new)
    open(f, 'w').write(s)
    print("FIXED: added 2 missing closing braces + iforest success message")
else:
    print("PATTERN NOT FOUND - may already be fixed or different text")
