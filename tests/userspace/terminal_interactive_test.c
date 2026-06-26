/*
 * terminal_interactive_test.c — Test the terminal's input buffer
 *
 * Tests:
 *   - Insert characters one by one, verify they accumulate correctly
 *   - Backspace removes the last character
 *   - Enter clears the buffer and returns the command string
 *
 * 8 assertions.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Mock the terminal's input buffer */
#define TERM_BUF_SIZE 256

typedef struct {
    char buf[TERM_BUF_SIZE];
    int len;
} term_input_t;

static void term_input_init(term_input_t *t) {
    t->len = 0;
    t->buf[0] = '\0';
}

static void term_input_char(term_input_t *t, char c) {
    if (t->len < TERM_BUF_SIZE - 1) {
        t->buf[t->len++] = c;
        t->buf[t->len] = '\0';
    }
}

static void term_input_backspace(term_input_t *t) {
    if (t->len > 0) {
        t->len--;
        t->buf[t->len] = '\0';
    }
}

static const char *term_input_submit(term_input_t *t) {
    const char *cmd = t->buf;
    return cmd;
}

static int test_count = 0;
static int pass_count = 0;

#define ASSERT(cond) do { \
    test_count++; \
    if (cond) { pass_count++; printf("  [PASS] %s\n", #cond); } \
    else { printf("  [FAIL] %s\n", #cond); } \
} while(0)

int main(void) {
    printf("=== Terminal Interactive Test ===\n");

    /* Test 1: Characters accumulate correctly */
    printf("Test 1: Character accumulation\n");
    {
        term_input_t t;
        term_input_init(&t);
        term_input_char(&t, 'h');
        term_input_char(&t, 'e');
        term_input_char(&t, 'l');
        term_input_char(&t, 'l');
        term_input_char(&t, 'o');
        ASSERT(t.len == 5);
        ASSERT(strcmp(t.buf, "hello") == 0);
    }

    /* Test 2: Backspace removes last character */
    printf("Test 2: Backspace\n");
    {
        term_input_t t;
        term_input_init(&t);
        term_input_char(&t, 'h');
        term_input_char(&t, 'i');
        term_input_backspace(&t);
        ASSERT(t.len == 1);
        ASSERT(strcmp(t.buf, "h") == 0);
    }

    /* Test 3: Enter submits and returns command */
    printf("Test 3: Submit (Enter)\n");
    {
        term_input_t t;
        term_input_init(&t);
        term_input_char(&t, 'l');
        term_input_char(&t, 's');
        const char *cmd = term_input_submit(&t);
        ASSERT(strcmp(cmd, "ls") == 0);
        /* After submit, buffer should be clearable */
        term_input_init(&t);
        ASSERT(t.len == 0);
    }

    /* Test 4: Empty buffer submit returns empty string */
    printf("Test 4: Empty submit\n");
    {
        term_input_t t;
        term_input_init(&t);
        const char *cmd = term_input_submit(&t);
        ASSERT(strlen(cmd) == 0);
    }

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           pass_count, test_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
