# Guardian Roadmap — Task → Exact Edit Map (source-verified)

Source: live read of all 43 guardian files. Each edit below cites real line numbers.

## PHASE 1 — Bug fixes (all EDITS to existing files)

### 1.1 inotify path fix — EDIT src/fim_engine.c
- Struct g_fim_engine (line 35-40): add `char wd_dirs[64][G_PATH_MAX];` + `int wd_used[64];`
- g_fim_add_watch (line 187): after inotify_add_watch, store wd→dirname(path). dirname mutates a copy.
- g_fim_watch_inotify (line 340): `snprintf(ev.path,"/%s",ie->name)` → `snprintf(ev.path,"%s/%s",wd_dirs[ie->wd],ie->name)` with bounds check on ie->wd.

### 1.2 Populate auth_binaries BPF map — EDIT src/guardian.c
- After line 970 (g_ebpf_load_all) inside g_daemon_run: add populate_auth_binaries(ee) helper.
- FNV-1a must match BPF hash_str (bpf.c:139 — 0xcbf29ce484222325 / 0x100000001b3). Hash the comm string WITH trailing NUL behavior matching BPF (BPF reads until NUL, stops).
- Names: passwd, login, sshd, pam_unix, chpasswd, gpasswd, su, sudo.

### 1.3 FIM restore from baseline — EDIT src/action_engine.c + NEW scripts/build-baseline.sh
- g_action_restore_file (line 105-131): BEFORE searching quarantine, check /etc/wlt/guardian/baseline/<escaped>. If exists, copy back. Only fall back to quarantine if no baseline.
- Emit FIM_RESTORE_NO_BASELINE ALERT event if neither exists.
- NEW scripts/build-baseline.sh: reads a paths file, copies each to baseline dir with slashes→__.

### 1.4 IPv6 connect hook — EDIT bpf/guardian.bpf.c
- tp_connect (line 270-300): after `if (family != 2) return 0;` — handle family==10 (AF_INET6).
- sockaddr_in6: family(2) + port(2) + flowinfo(4) + addr(16) at offset 8. Check ::1 (16 zero bytes except last=1). Skip loopback. Else same uid-kill logic.

## PHASE 2 — New detection (mix)

### 2.1 Canary engine — NEW src/canary_engine.c + EDIT include/guardian.h + EDIT src/guardian.c (main loop) + EDIT Makefile
- Header: add g_canary_engine_t + g_canary_init/verify_all/free.
- canary_engine.c: create canaries at /tmp/!GUARDIAN_CANARY_*.bin, /var/tmp/, /etc/ (1KB random). verify_all() stats each, returns count missing.
- guardian.c main loop: call g_canary_verify_all() every 30s; on missing → CRITICAL RANSOMWARE_CANARY_DELETED + freeze.
- Makefile: add canary_engine.o to DAEMON_SRCS (auto via wildcard — just need the .c).

### 2.2 Isolation Forest in C — NEW src/iforest.c + NEW ml/export_iforest.py + EDIT src/anomaly_engine.c + EDIT Makefile
- iforest.c: iforest_t struct + iforest_load(path) + iforest_score(f, features) per deep-research §12.2 algorithm.
- export_iforest.py: load anomaly.pkl, walk estimators_.tree_, serialize nodes (feature:int32, threshold:double, left:int32, right:int32, size:int32) + tree_roots. Write ml/anomaly.iforest + .sha256.
- anomaly_engine.c g_anomaly_init: if anomaly.iforest exists → load via iforest_load, set ae->use_native=true. call_python_model → iforest_score when native.
- Makefile: add iforest.o to daemon (wildcard picks it up).

### 2.3 eBPF feature counters — EDIT bpf/guardian.bpf.c + EDIT src/anomaly_engine.c + EDIT include/guardian.h (g_ebpf_get_map_value)
- BPF: add fork_count map (pid→u32, inc in tp_clone, del in sched_process_exit), write_count map (pid→u64, new tp_write hook), syscall_count (pid→u64, raw_tp/sys_enter).
- anomaly_engine.c collect_features: read fork_count→feat[4], write_count→feat[3], syscall_count→feat[7].
- Need g_ebpf_get_map_value(ee, name, key, out) helper in ebpf_engine.c.

### 2.4 Ransomware module — EDIT bpf/guardian.bpf.c (add unlink/rename/write hooks) + NEW src/ransomware_engine.c + EDIT guardian.c main loop + EDIT Makefile
- BPF: unlink_count, rename_count maps. tp_unlink/tp_unlinkat inc, tp_rename/tp_renameat inc.
- ransomware_engine.c: g_ransomware_check() reads maps, thresholds per roadmap.
- guardian.c: call every 5s.

### 2.5 Process ancestry IOA — EDIT bpf/guardian.bpf.c (ancestry map in tp_execve) + EDIT src/ebpf_engine.c (ringbuf_cb reads ppid) + EDIT rules/ebpf.rules (add 2 rules)
- BPF: ancestry map pid→{ppid,comm,parent_comm}. tp_execve: bpf_get_current_task→real_parent→read pid+comm.
- ebpf_engine.c ringbuf_cb: fill ev.ppid from ancestry lookup.
- ebpf.rules: add SHELL_SPAWNED_BY_WEB_SERVER, SHELL_SPAWNED_BY_PYTHON. (Need pcomm field in event — add to g_event_t? No — pcomm not in struct. Use rule condition on comm + path ancestor. Simplest: BPF emits parent_comm in path field for execve events. OR add pcomm[16] to g_event_t — but that changes ABI. Smart: store parent comm in ev.detail for execve. Let rules match detail contains.)

### 2.6 Compositor alerts — EDIT src/action_engine.c
- G_ACT_ALERT case (line 274): write JSON to /run/guardian/alerts.log (append). Create /run/guardian at init.

## PHASE 3 — WLTIOS integration (guardian-side artifacts only; WLTIOS files are OUTSIDE guardian folder — handoff doc instead)

### 3.1-3.4 — NEW wlt-integration/README.md + NEW wlt-integration/guardian.service + NEW wlt-integration/build-initramfs-snippet.sh
- Can't edit WLTIOS/src/userspace/wlt-init/init-minimal.c (outside guardian). Instead: precise patch in README showing exact boot order + diff.
- guardian.service: init script template for wlt-init to call.
- build-initramfs-snippet.sh: the exact bundle commands, sourced by build-initramfs64.sh.
- kill switch unify: document that wlt-shell cmd_killswitch should send to /run/guardian.sock.

### 3.5 — EDIT tests/run_all.sh (add WLTIOS integration tests, gated on $WLTIOS_MODE env)

## PHASE 4 — Advanced

### 4.1 BPF LSM migration — EDIT bpf/guardian.bpf.c (add lsm/ hooks alongside tracepoints, gated on CONFIG_BPF_LSM)
### 4.2 PMU engine — NEW src/pmu_engine.c + EDIT guardian.c + EDIT Makefile
### 4.3 YARA mem scan enhancement — EDIT src/sig_engine.c (scan deleted-file regions, /tmp-backed, exe symlink precheck)
### 4.4 Tor consensus tracking — EDIT src/net_engine.c (parse cached-consensus, guard set)
### 4.5 seL4 — BLOCKED (needs rebuild env) — document only

## PHASE 5 — Hardening (analysis + targeted edits)

### 5.1 TPM key sealing — EDIT src/forensic_log.c (key from TPM PCR via tpm2-tss if available, else urandom+warning) + doc
### 5.2 Cgroup isolation — wlt-integration/guardian-cgroup.conf
### 5.3 FIM RO mount — doc in wlt-integration/README
### 5.4 Threat model review — NEW docs/THREAT_MODEL.md
### 5.5 Memory safety audit — NEW docs/MEMORY_SAFETY_AUDIT.md + targeted fixes to scanf/format strings
### 5.6 Fuzzing harness — NEW tests/fuzz_mcp.py
### 5.7 Race condition audit — NEW docs/RACE_AUDIT.md + targeted mutex fixes

## BUILD STRATEGY
- After each phase: push all files to PC via /api/put, then WSL: cd guardian && source activate.sh && make clean && make all 2>&1. Fix errors. Commit when green.
- Phase 1 first (4 edits). Build. Commit. Then Phase 2 in sub-batches. etc.
