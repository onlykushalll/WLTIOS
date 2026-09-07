/* canary_engine.c — Phase 2.1: Ransomware canary file engine.
 *
 * Creates bait files in locations ransomware encrypts first: both system
 * dirs and, per real user under /home, the actual Documents/Pictures/
 * Desktop/Downloads locations a desktop ransomware attack targets. Any
 * access or modification = immediate CRITICAL alert + freeze. The
 * filename is derived per-machine (see derive_canary_name()) rather than
 * a single fixed string, so it can't be excluded by one hardcoded name
 * across every installation the way "!GUARDIAN_CANARY_DO_NOT_TOUCH.bin"
 * could - real ransomware families are documented checking for known
 * EDR canary patterns by name.
 *
 * The BPF LSM file_open hook (Phase 4.1) will inline-kill openers; until then
 * the userspace verify loop catches deletion/modification within 30s.
 */
#include "guardian.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <openssl/rand.h>

#define G_CANARY_SIZE 1024
#define G_MAX_CANARIES 24

struct g_canary {
    char path[G_PATH_MAX];
    uint8_t hash[32];   /* expected SHA-256 of canary contents */
    int present;
};

struct g_canary_engine {
    struct g_canary canaries[G_MAX_CANARIES];
    int n;
};

static void canary_hash(const uint8_t *data, size_t len, uint8_t out[32]) {
    g_sha256(data, len, out);
}

/* [improvement 2026-08-22] Real ransomware families are documented
 * checking process/file lists for known EDR canary naming patterns and
 * skipping them - a single fixed name shipped identically in every
 * installation is exactly the kind of thing such a check hardcodes
 * once and defeats everywhere. Derive a per-machine name instead:
 * deterministic from /etc/machine-id (same file -> same name across
 * restarts, so the existing O_EXCL-retry/re-hash logic below still
 * works), but different per installation, so no single exclusion
 * entry works across machines. Also picked to look like a real user
 * file (a photo/scan/document naming pattern), not an obvious
 * security-tool name. */
static void derive_canary_name(char *out, size_t out_sz) {
    uint8_t seed[128] = {0};
    size_t seed_len = 0;
    FILE *mf = fopen("/etc/machine-id", "r");
    if (mf) {
        seed_len = fread(seed, 1, sizeof(seed) - 32, mf);
        fclose(mf);
    }
    /* Salt so this hash never collides with any other derivation that
     * might also key off machine-id elsewhere in the system. */
    const char *salt = "guardian-canary-v1";
    size_t salt_len = strlen(salt);
    if (seed_len + salt_len < sizeof(seed)) {
        memcpy(seed + seed_len, salt, salt_len);
        seed_len += salt_len;
    }
    uint8_t h[32];
    g_sha256(seed, seed_len, h);
    static const char *prefixes[] = {"IMG_", "Scan_", "Document_", "Photo_", "Backup_", "Receipt_"};
    static const char *exts[]     = {".jpg", ".pdf", ".docx", ".png", ".zip", ".xlsx"};
    int pi = h[0] % (int)(sizeof(prefixes) / sizeof(prefixes[0]));
    int ei = h[1] % (int)(sizeof(exts) / sizeof(exts[0]));
    snprintf(out, out_sz, "%s%02x%02x%02x%02x%s",
             prefixes[pi], h[2], h[3], h[4], h[5], exts[ei]);
}

/* [improvement 2026-08-22] The original static list (/tmp, /var/tmp,
 * /etc, /root, /home) skips the directories an actual desktop
 * ransomware attack encrypts - a real user's Documents/Pictures/
 * Desktop/Downloads. Build the placement list dynamically: the
 * original system dirs, plus those four real subdirectories for every
 * user found under /home that actually has them. */
static int build_canary_dirs(char dirs[][G_PATH_MAX], int max_dirs) {
    int n = 0;
    static const char *system_dirs[] = { "/tmp", "/var/tmp", "/etc", "/root", NULL };
    for (int i = 0; system_dirs[i] && n < max_dirs; i++)
        g_strlcpy(dirs[n++], system_dirs[i], G_PATH_MAX);

    DIR *home = opendir("/home");
    if (home) {
        struct dirent *ent;
        static const char *subdirs[] = { "Documents", "Pictures", "Desktop", "Downloads", NULL };
        while ((ent = readdir(home)) != NULL && n < max_dirs) {
            if (ent->d_name[0] == '.') continue;
            for (int j = 0; subdirs[j] && n < max_dirs; j++) {
                char candidate[G_PATH_MAX];
                snprintf(candidate, sizeof(candidate), "/home/%s/%s", ent->d_name, subdirs[j]);
                struct stat st;
                if (stat(candidate, &st) == 0 && S_ISDIR(st.st_mode))
                    g_strlcpy(dirs[n++], candidate, G_PATH_MAX);
            }
        }
        closedir(home);
    }
    return n;
}

g_canary_engine_t *g_canary_init(void) {
    g_canary_engine_t *ce = calloc(1, sizeof(*ce));
    if (!ce) return NULL;

    char canary_name[128];
    derive_canary_name(canary_name, sizeof(canary_name));

    char dirs[G_MAX_CANARIES][G_PATH_MAX];
    int n_dirs = build_canary_dirs(dirs, G_MAX_CANARIES);

    for (int i = 0; i < n_dirs && ce->n < G_MAX_CANARIES; i++) {
        const char *dir = dirs[i];
        /* skip dirs we can't write to (non-root in dev sandbox) */
        if (access(dir, W_OK) != 0) continue;
        struct g_canary *c = &ce->canaries[ce->n];
        snprintf(c->path, sizeof(c->path), "%s/%s", dir, canary_name);

        /* generate 1KB random content */
        uint8_t buf[G_CANARY_SIZE];
        if (RAND_bytes(buf, sizeof(buf)) != 1) {
            /* fall back to urandom if RAND_bytes unavailable */
            FILE *f = fopen("/dev/urandom", "rb");
            if (!f || fread(buf, 1, sizeof(buf), f) != sizeof(buf)) {
                if (f) fclose(f);
                continue;
            }
            fclose(f);
        }
        canary_hash(buf, sizeof(buf), c->hash);

        /* write atomically: O_CREAT|O_EXCL prevents racing a pre-existing file */
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
        }
        ssize_t w = write(fd, buf, sizeof(buf));
        close(fd);
        if (w == sizeof(buf)) {
            c->present = 1;
            ce->n++;
            printf("[canary] placed %s\n", c->path);
        } else {
            unlink(c->path);
        }
    }
    printf("[canary] %d canary files placed\n", ce->n);
    return ce;
}

void g_canary_free(g_canary_engine_t *ce) {
    if (!ce) return;
    /* NOTE: we do NOT delete canary files on shutdown — they must persist
     * across daemon restarts so a ransomware attack between runs is detected. */
    free(ce);
}

/* Verify all canaries: check existence + content hash.
 * Returns number of missing/tampered canaries (0 = all intact). */
int g_canary_verify_all(g_canary_engine_t *ce) {
    if (!ce) return 0;
    int n_bad = 0;
    for (int i = 0; i < ce->n; i++) {
        struct g_canary *c = &ce->canaries[i];
        struct stat st;
        if (stat(c->path, &st) != 0) {
            /* canary deleted — ransomware indicator */
            n_bad++;
            g_event_t ev = {0};
            g_strlcpy(ev.source, "canary", sizeof(ev.source));
            g_strlcpy(ev.rule_id, "RANSOMWARE_CANARY_DELETED", sizeof(ev.rule_id));
            g_strlcpy(ev.path, c->path, sizeof(ev.path));
            ev.severity = G_SEV_CRITICAL;
            ev.verdict = G_VERDICT_MALICIOUS;
            ev.action_taken = G_ACT_FREEZE;  /* policy may upgrade to kill switch */
            ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
            ev.seq = g_bus_next_seq();
            snprintf(ev.detail, sizeof(ev.detail),
                     "Canary file %s deleted — likely ransomware mass-delete/encrypt",
                     c->path);
            g_bus_publish(&ev);
            c->present = 0;
            continue;
        }
        /* check size + hash */
        if (st.st_size != G_CANARY_SIZE) {
            n_bad++;
            g_event_t ev = {0};
            g_strlcpy(ev.source, "canary", sizeof(ev.source));
            g_strlcpy(ev.rule_id, "RANSOMWARE_CANARY_MODIFIED", sizeof(ev.rule_id));
            g_strlcpy(ev.path, c->path, sizeof(ev.path));
            ev.severity = G_SEV_CRITICAL;
            ev.verdict = G_VERDICT_MALICIOUS;
            ev.action_taken = G_ACT_FREEZE;
            ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
            ev.seq = g_bus_next_seq();
            snprintf(ev.detail, sizeof(ev.detail),
                     "Canary file %s size changed (%ld != %d) — tampering",
                     c->path, (long)st.st_size, G_CANARY_SIZE);
            g_bus_publish(&ev);
            continue;
        }
        /* full content hash check */
        int fd = open(c->path, O_RDONLY);
        if (fd < 0) continue;
        uint8_t buf[G_CANARY_SIZE];
        ssize_t r = read(fd, buf, sizeof(buf));
        close(fd);
        if (r != G_CANARY_SIZE) continue;
        uint8_t h[32];
        canary_hash(buf, r, h);
        if (memcmp(h, c->hash, 32) != 0) {
            n_bad++;
            g_event_t ev = {0};
            g_strlcpy(ev.source, "canary", sizeof(ev.source));
            g_strlcpy(ev.rule_id, "RANSOMWARE_CANARY_ENCRYPTED", sizeof(ev.rule_id));
            g_strlcpy(ev.path, c->path, sizeof(ev.path));
            ev.severity = G_SEV_CRITICAL;
            ev.verdict = G_VERDICT_MALICIOUS;
            ev.action_taken = G_ACT_KILL_SWITCH;
            ev.ts_ns = (uint64_t)time(NULL) * 1000000000ULL;
            ev.seq = g_bus_next_seq();
            snprintf(ev.detail, sizeof(ev.detail),
                     "Canary file %s content hash mismatch — encryption detected, activating kill switch",
                     c->path);
            g_bus_publish(&ev);
        }
    }
    return n_bad;
}
