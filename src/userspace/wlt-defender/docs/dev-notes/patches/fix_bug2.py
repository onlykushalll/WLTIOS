import sys
f = sys.argv[1]
s = open(f).read()

old = """        /* write atomically: O_CREAT|O_EXCL prevents racing a pre-existing file */
        int fd = open(c->path, O_WRONLY | O_CREAT | O_EXCL, 0400);
        if (fd < 0) {
            /* already exists (re-run) — verify it instead */
            struct stat st;
            if (stat(c->path, &st) == 0 && st.st_size == G_CANARY_SIZE) {
                c->present = 1;
                ce->n++;
            }
            continue;
        }"""

new = """        /* write atomically: O_CREAT|O_EXCL prevents racing a pre-existing file */
        int fd = open(c->path, O_WRONLY | O_CREAT | O_EXCL, 0400);
        if (fd < 0) {
            /* BUG 2 fix: already exists (re-run) — READ + HASH it so verify
             * works. Old code set present=1 but left c->hash all-zero, so
             * every restart triggered RANSOMWARE_CANARY_ENCRYPTED false positives. */
            struct stat st;
            if (stat(c->path, &st) == 0 && st.st_size == G_CANARY_SIZE) {
                int rfd = open(c->path, O_RDONLY);
                if (rfd >= 0) {
                    uint8_t rbuf[G_CANARY_SIZE];
                    if (read(rfd, rbuf, sizeof(rbuf)) == G_CANARY_SIZE) {
                        canary_hash(rbuf, sizeof(rbuf), c->hash);
                        c->present = 1;
                        ce->n++;
                    }
                    close(rfd);
                }
            }
            continue;
        }"""

assert old in s, "canary O_EXCL branch not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 2 fixed: canary hash loaded on restart")
