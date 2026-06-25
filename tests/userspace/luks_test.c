/**
 * @file luks_test.c
 * @brief Tests for LUKS encryption parameter validation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0, g_fail = 0;
#define TEST_ASSERT(c,m) do{if(c){g_pass++;printf("  PASS: %s\n",m);}else{g_fail++;printf("  FAIL: %s\n",m);}}while(0)

static int validate_luks_format(const char *cmd)
{
    if (!strstr(cmd, "luksFormat")) return 0;
    if (!strstr(cmd, "--type luks2")) return 0;
    return 1;
}

static int validate_luks_open(const char *cmd)
{
    if (!strstr(cmd, "luksOpen")) return 0;
    if (!strstr(cmd, "/dev/")) return 0;
    return 1;
}

static int validate_pbkdf(const char *cmd)
{
    if (strstr(cmd, "--pbkdf argon2id")) return 1;
    if (strstr(cmd, "--pbkdf pbkdf2")) return 1;
    return 0;
}

int main(void)
{
    printf("=== LUKS Encryption Tests ===\n\n");

    printf("--- Format validation ---\n");
    TEST_ASSERT(validate_luks_format("cryptsetup luksFormat --type luks2 /dev/vda") == 1, "Valid format command");
    TEST_ASSERT(validate_luks_format("cryptsetup format /dev/vda") == 0, "Missing luksFormat rejected");
    TEST_ASSERT(validate_luks_format("cryptsetup luksFormat /dev/vda") == 0, "Missing --type rejected");

    printf("\n--- Open validation ---\n");
    TEST_ASSERT(validate_luks_open("cryptsetup luksOpen /dev/vda wlt-data") == 1, "Valid open command");
    TEST_ASSERT(validate_luks_open("cryptsetup open wlt-data") == 0, "Missing /dev/ rejected");
    TEST_ASSERT(validate_luks_open("cryptsetup luksOpen wlt-data") == 0, "Missing device path rejected");

    printf("\n--- PBKDF validation ---\n");
    TEST_ASSERT(validate_pbkdf("--pbkdf argon2id") == 1, "argon2id accepted");
    TEST_ASSERT(validate_pbkdf("--pbkdf pbkdf2") == 1, "pbkdf2 accepted");
    TEST_ASSERT(validate_pbkdf("--pbkdf sha256") == 0, "sha256 rejected");
    TEST_ASSERT(validate_pbkdf("no pbkdf here") == 0, "Missing pbkdf rejected");

    printf("\n--- Full command construction ---\n");
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cryptsetup luksFormat --type luks2 --pbkdf argon2id /dev/vda");
    TEST_ASSERT(validate_luks_format(cmd) == 1, "Full format command valid");
    TEST_ASSERT(validate_pbkdf(cmd) == 1, "Full format has valid PBKDF");

    snprintf(cmd, sizeof(cmd), "cryptsetup luksOpen /dev/vda wlt-data");
    TEST_ASSERT(validate_luks_open(cmd) == 1, "Full open command valid");

    printf("\n--- Lock/unlock commands ---\n");
    const char *lock_cmd = "cryptsetup luksClose wlt-data";
    const char *unlock_cmd = "cryptsetup luksOpen /dev/vda wlt-data";
    TEST_ASSERT(strstr(lock_cmd, "luksClose") != NULL, "Lock uses luksClose");
    TEST_ASSERT(strstr(unlock_cmd, "luksOpen") != NULL, "Unlock uses luksOpen");
    TEST_ASSERT(strstr(unlock_cmd, "wlt-data") != NULL, "Unlock targets wlt-data mapper");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}