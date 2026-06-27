# Guardian Integration Test Results
**Date:** 2026-06-27 16:46:40 UTC

## 1. Clean build + self-test (all fixes in place)

Build errors above (empty = clean). Running self-test...
```
[test] Verifying CRITICAL events bypass deduplication...
[test] CRITICAL bypass deduplication passed.
[test] Verifying per-rule rate limiting...
[test] Per-rule rate limiting passed.
[test] Verifying global rate limiting...
[test] Global rate limiting passed.
[test] Verifying model hash verification...
[test] Model hash verification passed.
[test] Verifying log quota and overflow spillover...
[test] Log quota overflow spillover passed.
[test] Verifying event propagation and BPF action mapping (Phase 2)...
[test] Event propagation and BPF action mapping passed.
[test] Verifying FIM DB encryption and tampering detection (Phase 3)...
[test] FIM DB encryption and tampering detection passed.
[test] Verifying API socket credentials authentication and delegation (Phase 3)...
[test] API socket authentication and delegation passed.
[test] Verifying process memory scanning on clean process (PID 1)...
[test] Skipping process memory scanning self-test (no signature database found).
[test] Verifying signed YARA updates returns gracefully when disabled...
[test] Canary restart false-positive test...
[test] Canary restart test passed (no false positives after re-init).
[test] Isolation Forest inference correctness test...
[iforest] loaded: 80 trees, 8 features, 11418 nodes, threshold=0.600
[test]   iforest normal=0.3883 anomalous=0.7529
[test] Isolation Forest inference correctness passed (anomalous > normal).
[test] Ransomware rate-based detection test...
[test]   confirmed: 5 unlinks/5s for 20 cycles = 100 cumulative but only 60/min rate
[test]   rate-based: does NOT fire (correct). cumulative-based: would fire (bug).
[test] Ransomware rate-based detection passed.
[guardian] self-test OK
```

**Self-test result:** PASS

## 2. Regression check: iforest packed struct (BUG 3)
Temporarily revert the packed attribute, rebuild, confirm test 13 fails, restore.

```
```
**Iforest regression:** test did NOT catch the break

## 3. Regression check: canary hash on restart (BUG 2)
Temporarily revert the hash-on-restart fix, rebuild, confirm test 12 fails, restore.

reverted canary hash-on-restart
```
FAIL: canary restart produced 2 false positive(s) - hash not loaded
[test] Canary restart false-positive test...
```
**Canary regression:** test FAILED when broken (has teeth)

## 4. Restore all fixes + final clean build

```
[guardian] self-test OK
```

## Summary
- Self-test (all fixes): PASS
- Iforest regression teeth: NO
- Canary regression teeth: YES
