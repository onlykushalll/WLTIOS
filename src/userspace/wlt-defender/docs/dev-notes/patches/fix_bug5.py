import sys
f = sys.argv[1]
s = open(f).read()

# BUG 5 fix: PERF_COUNT_HW_CACHE_L1D -> PERF_COUNT_HW_CACHE_LL (last-level cache)
# L1D miss rate is 10-50% for normal code; the >0.05 threshold would flag every process.
old = """    /* L1-dcache load-miss: PERF_COUNT_HW_CACHE_L1D | READ | MISS */
    cm.config = (PERF_COUNT_HW_CACHE_L1D) |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);"""
new = """    /* BUG 5 fix: PERF_COUNT_HW_CACHE_L1D -> PERF_COUNT_HW_CACHE_LL.
     * L1D miss rate is 10-50% for normal code (would false-positive every
     * process). LLC (last-level cache) misses are the real side-channel signal. */
    cm.config = (PERF_COUNT_HW_CACHE_LL) |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);"""
assert old in s, "PMU L1D config not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 5 fixed: PMU uses LLC (last-level cache) not L1D")
