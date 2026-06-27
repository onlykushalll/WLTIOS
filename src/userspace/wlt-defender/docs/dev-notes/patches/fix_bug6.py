import sys
f = sys.argv[1]
s = open(f).read()

# BUG 6 fix: copy_file_baseline preserves perms + ownership + uses atomic temp+rename.
old = """static int copy_file_baseline(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -errno;
    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (dfd < 0) { close(sfd); return -errno; }
    char buf[65536]; ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(dfd, buf, n);
        if (w != n) { close(sfd); close(dfd); return -EIO; }
    }
    close(sfd); close(dfd);
    return 0;
}"""
new = """/* BUG 6 fix: copy baseline -> target preserving mode+owner, atomically.
 * Old code copied bytes only — restored /etc/passwd got mode 0600 + the
 * daemon's uid/gid instead of 0644 root:root, breaking login. Also no
 * atomic temp+rename, so a crashed restore left a corrupted target. */
static int copy_file_baseline(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return -errno;

    /* write to a .tmp sibling, then rename for atomicity */
    char tmp[G_PATH_MAX * 2];
    snprintf(tmp, sizeof(tmp), "%s.guardian-restore.tmp", dst);
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -errno;
    int dfd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (dfd < 0) { close(sfd); return -errno; }
    char buf[65536]; ssize_t n;
    int rc = 0;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(dfd, buf, n);
        if (w != n) { rc = -EIO; break; }
    }
    close(sfd);
    if (fsync(dfd) != 0) rc = -EIO;
    close(dfd);
    if (rc != 0) { unlink(tmp); return rc; }

    /* preserve ownership + mode */
    if (chmod(tmp, st.st_mode & 07777) != 0) { unlink(tmp); return -errno; }
    if (chown(tmp, st.st_uid, st.st_gid) != 0) {
        /* chown may fail if we're not root — non-fatal, keep daemon's owner */
    }
    /* atomic replace */
    if (rename(tmp, dst) != 0) { unlink(tmp); return -errno; }
    return 0;
}"""
assert old in s, "copy_file_baseline not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 6 fixed: restore preserves perms/owner + atomic temp+rename")
