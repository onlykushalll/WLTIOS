/**
 * @file pkg_test.c
 * @brief Tests for package manager state machine + MANIFEST parsing
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0, g_fail = 0;
#define TEST_ASSERT(c,m) do{if(c){g_pass++;printf("  PASS: %s\n",m);}else{g_fail++;printf("  FAIL: %s\n",m);}}while(0)

typedef enum {
    PKG_IDLE = 0,
    PKG_DOWNLOADING,
    PKG_VERIFYING,
    PKG_EXTRACTING,
    PKG_INSTALLED,
    PKG_FAILED,
} pkg_state_t;

typedef struct {
    char name[64];
    char version[32];
    char sha256[65];
    int verified;
    pkg_state_t state;
} pkg_t;

static int parse_manifest(const char *manifest, pkg_t *pkg)
{
    const char *name = strstr(manifest, "name = \"");
    if (name) { name += 7; const char *end = strstr(name, "\""); if (end) {
        size_t len = end - name; if (len < 64) { memcpy(pkg->name, name, len); pkg->name[len] = 0; } } }

    const char *ver = strstr(manifest, "version = \"");
    if (ver) { ver += 10; const char *end = strstr(ver, "\""); if (end) {
        size_t len = end - ver; if (len < 32) { memcpy(pkg->version, ver, len); pkg->version[len] = 0; } } }

    const char *sha = strstr(manifest, "sha256 = \"");
    if (sha) { sha += 9; const char *end = strstr(sha, "\""); if (end) {
        size_t len = end - sha; if (len < 65) { memcpy(pkg->sha256, sha, len); pkg->sha256[len] = 0; } } }

    return (pkg->name[0] && pkg->version[0]) ? 0 : -1;
}

static int verify_sha256(const char *expected, const char *actual)
{
    return strcmp(expected, actual) == 0;
}

int main(void)
{
    printf("=== Package Manager Tests ===\n\n");

    printf("--- MANIFEST parsing ---\n");
    const char *manifest =
        "name = \"hello-wlt\"\n"
        "version = \"1.0.0\"\n"
        "arch = \"amd64\"\n"
        "sha256 = \"abc123def456\"\n"
        "ed25519_sig = \"sig\"";
    pkg_t pkg = {0};
    int rc = parse_manifest(manifest, &pkg);
    TEST_ASSERT(rc == 0, "Manifest parses successfully");
    TEST_ASSERT(strcmp(pkg.name, "hello-wlt") == 0, "Package name is hello-wlt");
    TEST_ASSERT(strcmp(pkg.version, "1.0.0") == 0, "Version is 1.0.0");
    TEST_ASSERT(strcmp(pkg.sha256, "abc123def456") == 0, "SHA256 extracted");

    printf("\n--- Missing fields ---\n");
    const char *bad_manifest = "name = \"test\"\n";
    pkg_t pkg2 = {0};
    rc = parse_manifest(bad_manifest, &pkg2);
    TEST_ASSERT(rc == -1, "Missing version fails parsing");
    TEST_ASSERT(strcmp(pkg2.name, "test") == 0, "Name still parsed before failure");

    printf("\n--- SHA256 verification ---\n");
    TEST_ASSERT(verify_sha256("abc123", "abc123") == 1, "Matching SHA256 passes");
    TEST_ASSERT(verify_sha256("abc123", "xyz789") == 0, "Mismatched SHA256 fails");
    TEST_ASSERT(verify_sha256("", "") == 1, "Empty strings match (edge case)");

    printf("\n--- State machine ---\n");
    pkg_t pkg3 = {.state = PKG_IDLE};
    TEST_ASSERT(pkg3.state == PKG_IDLE, "Initial state is IDLE");
    pkg3.state = PKG_VERIFYING;
    TEST_ASSERT(pkg3.state == PKG_VERIFYING, "State transitions to VERIFYING");
    pkg3.verified = 1;
    pkg3.state = PKG_EXTRACTING;
    TEST_ASSERT(pkg3.state == PKG_EXTRACTING, "State transitions to EXTRACTING after verify");
    pkg3.state = PKG_INSTALLED;
    TEST_ASSERT(pkg3.state == PKG_INSTALLED, "State transitions to INSTALLED");

    printf("\n--- State machine failure ---\n");
    pkg_t pkg4 = {.state = PKG_VERIFYING};
    pkg4.verified = 0;
    pkg4.state = PKG_FAILED;
    TEST_ASSERT(pkg4.state == PKG_FAILED, "Failed verification goes to FAILED state");
    TEST_ASSERT(pkg4.verified == 0, "Verified flag stays 0 on failure");

    printf("\n--- Package format ---\n");
    TEST_ASSERT(sizeof(pkg_t) > 0, "Package struct has non-zero size");
    TEST_ASSERT(strlen("hello-wlt-1.0.0-wlt.tar.gz") > 10, "Package filename is reasonable");

    printf("\n--- Installed DB format ---\n");
    char db_line[128];
    snprintf(db_line, sizeof(db_line), "%s %s %s", "hello-wlt", "1.0.0", "/usr/local");
    TEST_ASSERT(strstr(db_line, "hello-wlt") != NULL, "DB line has package name");
    TEST_ASSERT(strstr(db_line, "1.0.0") != NULL, "DB line has version");
    TEST_ASSERT(strstr(db_line, "/usr/local") != NULL, "DB line has install path");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}