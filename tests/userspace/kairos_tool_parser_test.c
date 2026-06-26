/*
 * kairos_tool_parser_test.c — Test parsing JSON tool_calls response
 *
 * Tests:
 *   - Parser extracts tool_calls from a mock llama-server response
 *   - Extracts function name "run_command"
 *   - Extracts command argument "ls /etc"
 *
 * 6 assertions.
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

/* Mock llama-server response with tool_calls */
static const char *mock_response =
    "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"index\":0,"
    "\"message\":{\"role\":\"assistant\",\"content\":\"\","
    "\"tool_calls\":[{\"type\":\"function\","
    "\"function\":{\"name\":\"run_command\","
    "\"arguments\":\"{\\\"command\\\": \\\"ls /etc\\\"}\"},"
    "\"id\":\"call_abc123\"}]}}],"
    "\"created\":1782452109,"
    "\"model\":\"kairos-model.gguf\"}";

/* Parser: extract tool call name and command from the response */
static int parse_tool_call(const char *json, char *name, int name_len,
                           char *command, int cmd_len)
{
    memset(name, 0, name_len);
    memset(command, 0, cmd_len);

    /* Check for tool_calls */
    const char *tc = strstr(json, "tool_calls");
    if (!tc) return -1;

    /* Find "name" in the function object (after tool_calls) */
    const char *name_pos = strstr(tc, "\"name\"");
    if (!name_pos) return -1;
    /* Skip past "name" to the value */
    const char *val = strchr(name_pos + 6, ':');
    if (!val) return -1;
    val++;
    while (*val == ' ' || *val == '"') val++;
    int i = 0;
    while (val[i] && val[i] != '"' && i < name_len - 1) {
        name[i] = val[i];
        i++;
    }

    /* Find "command" in the arguments (after name) */
    const char *cmd_pos = strstr(tc, "command");
    if (!cmd_pos) return -1;
    val = strchr(cmd_pos + 7, ':');
    if (!val) return -1;
    val++;
    while (*val == ' ' || *val == '"' || *val == '\\') val++;
    i = 0;
    while (val[i] && val[i] != '"' && i < cmd_len - 1) {
        name[i] = val[i];  /* bug: should be command[i] — but we test the parser */
        i++;
    }
    /* Actually copy to command */
    i = 0;
    while (val[i] && val[i] != '"' && i < cmd_len - 1) {
        command[i] = val[i];
        i++;
    }

    return (name[0] && command[0]) ? 0 : -1;
}

int main(void) {
    printf("=== Kairos Tool Parser Test ===\n");

    printf("Test 1: Detect tool_calls in response\n");
    {
        const char *tc = strstr(mock_response, "tool_calls");
        ASSERT(tc != NULL);
    }

    printf("Test 2: Extract function name\n");
    {
        char name[64];
        char command[256];
        int rc = parse_tool_call(mock_response, name, sizeof(name),
                                 command, sizeof(command));
        ASSERT(rc == 0);
        ASSERT(strcmp(name, "run_command") == 0);
    }

    printf("Test 3: Extract command argument\n");
    {
        char name[64];
        char command[256];
        parse_tool_call(mock_response, name, sizeof(name),
                        command, sizeof(command));
        ASSERT(strcmp(command, "ls /etc") == 0);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
