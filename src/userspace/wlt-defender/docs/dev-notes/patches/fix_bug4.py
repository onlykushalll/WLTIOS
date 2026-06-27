import sys
f = sys.argv[1]
s = open(f).read()

# BUG 4 fix: implement rate-based detection using the prev_* slots table.
# The old code checked cumulative totals (unlink > 100) which false-positived
# on any long-running process. Now: compute delta since last check, scale to
# per-minute rate, compare to threshold.

old = """/* Scan /proc for PIDs, read their BPF counters, apply thresholds.
 * Returns number of ransomware alerts emitted. */
int g_ransomware_check(g_ransomware_engine_t *re) {
    if (!re) return -1;
    time_t now = time(NULL);
    int dt = (int)(now - re->last_check);
    if (dt < 1) dt = 1;
    re->last_check = now;
    int n_alerts = 0;

    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit(de->d_name[0])) continue;
        __u32 pid = (__u32)atoi(de->d_name);
        if (pid <= 1) continue;

        __u32 unlink = 0, rename = 0;
        __u64 writeb = 0;
        read_u32_map("unlink_count", pid, &unlink);
        read_u32_map("rename_count", pid, &rename);
        read_u64_map("write_bytes", pid, &writeb);
        if (unlink == 0 && rename == 0 && writeb == 0) continue;

        /* compute per-minute rates based on delta since last check */
        char detail[256];
        /* bulk delete: >100 unlinks total (threshold on cumulative count) */
        if (unlink > 100) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %u file deletions (bulk delete — possible ransomware)", pid, unlink);
            emit_event("BULK_DELETE_SUSPICIOUS", G_SEV_CRITICAL, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
        /* rename storm: >10 renames total with extension changes (heuristic) */
        if (rename > 10) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %u renames (rename storm — possible extension-changing ransomware)", pid, rename);
            emit_event("RENAME_STORM_SUSPICIOUS", G_SEV_HIGH, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
        /* high write rate: >1MB written total (entropy check would need /proc/pid/mem sampling) */
        if (writeb > 1024 * 1024) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %llu bytes written (high I/O — ransomware pattern check)", pid, (unsigned long long)writeb);
            emit_event("RANSOMWARE_IO_PATTERN", G_SEV_CRITICAL, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
    }
    closedir(d);
    return n_alerts;
}"""

new = """/* BUG 4 fix: find-or-create a slot for a pid in the prev-counters table.
 * Returns the slot index, or -1 if the table is full. */
static int find_slot(g_ransomware_engine_t *re, __u32 pid) {
    for (int i = 0; i < re->n; i++) {
        if (re->slots[i].pid == pid) return i;
    }
    if (re->n < 64) {
        re->slots[re->n].pid = pid;
        re->slots[re->n].prev_unlink = 0;
        re->slots[re->n].prev_rename = 0;
        re->slots[re->n].prev_write = 0;
        re->slots[re->n].first_seen = time(NULL);
        return re->n++;
    }
    return -1;
}

/* Scan /proc for PIDs, read their BPF counters, apply RATE thresholds.
 * BUG 4 fix: old code checked cumulative totals (unlink > 100) which
 * false-positived on any long-running process (apt clean, log rotators).
 * Now: compute delta since last check, scale to per-minute rate. */
int g_ransomware_check(g_ransomware_engine_t *re) {
    if (!re) return -1;
    time_t now = time(NULL);
    int dt = (int)(now - re->last_check);
    if (dt < 1) dt = 1;
    re->last_check = now;
    int n_alerts = 0;
    double dt_min = dt / 60.0;

    DIR *d = opendir("/proc");
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit(de->d_name[0])) continue;
        __u32 pid = (__u32)atoi(de->d_name);
        if (pid <= 1) continue;

        __u32 unlink = 0, rename = 0;
        __u64 writeb = 0;
        read_u32_map("unlink_count", pid, &unlink);
        read_u32_map("rename_count", pid, &rename);
        read_u64_map("write_bytes", pid, &writeb);

        int si = find_slot(re, pid);
        if (si < 0) continue;

        /* compute deltas (this check - last check) */
        __u32 d_unlink = (unlink > re->slots[si].prev_unlink) ? (unlink - re->slots[si].prev_unlink) : 0;
        __u32 d_rename = (rename > re->slots[si].prev_rename) ? (rename - re->slots[si].prev_rename) : 0;
        __u64 d_write  = (writeb > re->slots[si].prev_write) ? (writeb - re->slots[si].prev_write) : 0;

        /* update prev for next cycle */
        re->slots[si].prev_unlink = unlink;
        re->slots[si].prev_rename = rename;
        re->slots[si].prev_write = writeb;

        /* skip if no activity this interval */
        if (d_unlink == 0 && d_rename == 0 && d_write == 0) continue;

        /* per-minute rates */
        double unlink_per_min = d_unlink / dt_min;
        double rename_per_min = d_rename / dt_min;
        double write_per_min  = (double)d_write / dt_min;

        char detail[256];
        /* bulk delete: >100 unlinks/min (was: cumulative — false positive) */
        if (unlink_per_min > 100.0) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %.0f file deletions/min (bulk delete — possible ransomware)", pid, unlink_per_min);
            emit_event("BULK_DELETE_SUSPICIOUS", G_SEV_CRITICAL, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
        /* rename storm: >10 renames/min with extension changes (heuristic) */
        if (rename_per_min > 10.0) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %.0f renames/min (rename storm — possible extension-changing ransomware)", pid, rename_per_min);
            emit_event("RENAME_STORM_SUSPICIOUS", G_SEV_HIGH, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
        /* high write rate: >1MB/min (entropy check would need /proc/pid/mem sampling) */
        if (write_per_min > (1024.0 * 1024.0)) {
            snprintf(detail, sizeof(detail),
                     "PID %u: %.1f MB written/min (high I/O — ransomware pattern check)", pid, write_per_min/(1024*1024));
            emit_event("RANSOMWARE_IO_PATTERN", G_SEV_CRITICAL, G_ACT_FREEZE, pid, detail);
            n_alerts++;
        }
    }
    closedir(d);
    return n_alerts;
}"""

assert old in s, "ransomware_check not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 4 fixed: rate-based ransomware detection (delta + per-minute)")
