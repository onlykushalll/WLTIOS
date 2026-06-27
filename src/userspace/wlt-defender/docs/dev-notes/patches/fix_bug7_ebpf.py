import sys
f = sys.argv[1]  # src/ebpf_engine.c
s = open(f).read()

# BUG 7 fix part 2: populate ev.pcomm from the ancestry BPF map in ringbuf_cb.
# The ancestry map (keyed by pid) holds {ppid, parent_comm[16]}. We look it up
# for each event and copy parent_comm into ev.pcomm.
old = """    g_event_t ev = {0};
    ev.pid = be->pid;
    ev.ppid = be->ppid;
    ev.uid = be->uid;
    memcpy(ev.comm, be->comm, 16);
    g_strlcpy(ev.path, (const char *)be->path, sizeof(ev.path));"""
new = """    g_event_t ev = {0};
    ev.pid = be->pid;
    ev.ppid = be->ppid;
    ev.uid = be->uid;
    memcpy(ev.comm, be->comm, 16);
    g_strlcpy(ev.path, (const char *)be->path, sizeof(ev.path));
    /* BUG 7 fix: populate pcomm from the ancestry map so IOA rules can match
     * parent comm (e.g. SHELL_SPAWNED_BY_WEB_SERVER matches pcomm in nginx). */
#ifdef G_HAVE_LIBBPF
    if (g_ee_global) {
        struct g_ancestry {
            uint32_t ppid;
            uint8_t  parent_comm[16];
        } a;
        if (g_ebpf_get_map_value(g_ee_global, "ancestry", &be->pid, &a) == 0) {
            memcpy(ev.pcomm, a.parent_comm, 16);
            ev.pcomm[15] = 0;
        }
    }
#endif"""
assert old in s, "ringbuf_cb ev population not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 7 part 2: ringbuf_cb populates pcomm from ancestry map")
