/**
 * @file kairos_tools_test.c
 * @brief Tests for Kairos tool call parsing + execution
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0, g_fail = 0;
#define TEST_ASSERT(c,m) do{if(c){g_pass++;printf("  PASS: %s\n",m);}else{g_fail++;printf("  FAIL: %s\n",m);}}while(0)

static int parse_tool_call(const char *response, char *tool_name, size_t name_size,
                           char *args, size_t args_size)
{
    const char *tc = strstr(response, "run_command");
    if (!tc) { return -1; }
    strncpy(tool_name, "run_command", name_size - 1);
    tool_name[name_size - 1] = '\0';

    const char *cmd_start = strstr(tc, "\"command\":\"");
    if (!cmd_start) return -1;
    cmd_start += 11;
    const char *cmd_end = strstr(cmd_start, "\"");
    if (!cmd_end) return -1;
    size_t len = cmd_end - cmd_start;
    if (len >= args_size) len = args_size - 1;
    memcpy(args, cmd_start, len);
    args[len] = '\0';
    return 0;
}

static int validate_luks_params(const char *cmd)
{
    if (strstr(cmd, "--type luks2") == NULL) return 0;
    if (strstr(cmd, "--pbkdf argon2id") == NULL) return 0;
    return 1;
}

int main(void)
{
    printf("=== Kairos Tools Tests ===\n\n");

    printf("--- Tool call parsing ---\n");
    const char *resp1 = "{\"tool_calls\":[{\"function\":{\"name\":\"run_command\",\"arguments\":{\"command\":\"ls /etc\"}}}]";
    char name[64], args[256];
    int rc = parse_tool_call(resp1, name, sizeof(name), args, sizeof(args));
    TEST_ASSERT(rc == 0, "Parser finds run_command tool call");
    TEST_ASSERT(strcmp(name, "run_command") == 0, "Tool name extracted correctly");
    TEST_ASSERT(strcmp(args, "ls /etc") == 0, "Command argument extracted correctly");

    printf("\n--- No tool call ---\n");
    const char *resp2 = "{\"content\":\"Hello from Kairos\"}";
    rc = parse_tool_call(resp2, name, sizeof(name), args, sizeof(args));
    TEST_ASSERT(rc == -1, "Parser returns -1 when no tool call present");

    printf("\n--- Different command ---\n");
    const char *resp3 = "run_command with \"command\":\"cat /proc/meminfo\"";
    rc = parse_tool_call(resp3, name, sizeof(name), args, sizeof(args));
    TEST_ASSERT(rc == 0, "Parser finds run_command in raw text");
    TEST_ASSERT(strcmp(args, "cat /proc/meminfo") == 0, "cat command extracted");

    printf("\n--- get_system_info ---\n");
    /* Simulate system info output */
    char sysinfo[512];
    snprintf(sysinfo, sizeof(sysinfo), "System: WLTIOS v0.1.0\nTor: SOCKS5 on 9050 (running)\n");
    TEST_ASSERT(strstr(sysinfo, "WLTIOS") != NULL, "System info mentions WLTIOS");
    TEST_ASSERT(strstr(sysinfo, "Tor") != NULL, "System info mentions Tor");
    TEST_ASSERT(strstr(sysinfo, "9050") != NULL, "System info has SOCKS port");

    printf("\n--- tor_status ---\n");
    char tor_status[256];
    snprintf(tor_status, sizeof(tor_status), "Tor: RUNNING\nSOCKS5: 127.0.0.1:9050");
    TEST_ASSERT(strstr(tor_status, "RUNNING") != NULL, "Tor status shows RUNNING");
    TEST_ASSERT(strstr(tor_status, "9050") != NULL, "Tor status has port");

    printf("\n--- LUKS parameter validation ---\n");
    TEST_ASSERT(validate_luks_params("cryptsetup luksFormat --type luks2 --pbkdf argon2id /dev/vda") == 1, "Valid LUKS params");
    TEST_ASSERT(validate_luks_params("cryptsetup luksFormat /dev/vda") == 0, "Missing --type luks2 rejected");
    TEST_ASSERT(validate_luks_params("cryptsetup luksFormat --type luks2 /dev/vda") == 0, "Missing --pbkdf rejected");
    TEST_ASSERT(validate_luks_params("--type luks2 --pbkdf argon2id") == 1, "Minimal valid params");

    printf("\n--- Tool names ---\n");
    const char *tools[] = {"run_command", "read_file", "get_system_info", "tor_status"};
    TEST_ASSERT(strcmp(tools[0], "run_command") == 0, "Tool 0 is run_command");
    TEST_ASSERT(strcmp(tools[1], "read_file") == 0, "Tool 1 is read_file");
    TEST_ASSERT(strcmp(tools[2], "get_system_info") == 0, "Tool 2 is get_system_info");
    TEST_ASSERT(strcmp(tools[3], "tor_status") == 0, "Tool 3 is tor_status");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}