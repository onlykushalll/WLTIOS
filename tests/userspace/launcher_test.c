/**
 * @file launcher_test.c
 * @brief Unit tests for app launcher state machine
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0;
static int g_fail = 0;
#define TEST_ASSERT(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } \
} while(0)

typedef enum {
    LAUNCHER_CLOSED = 0,
    LAUNCHER_OPEN,
    LAUNCHER_SELECTING,
} launcher_state_t;

typedef struct {
    launcher_state_t state;
    int selected_option;
    int num_options;
} launcher_t;

static void launcher_open(launcher_t *l) {
    l->state = LAUNCHER_OPEN;
    l->selected_option = 0;
}

static void launcher_select(launcher_t *l, int option) {
    if (l->state != LAUNCHER_OPEN) return;
    if (option < 1 || option > l->num_options) return;
    l->selected_option = option;
    l->state = LAUNCHER_SELECTING;
}

static void launcher_close(launcher_t *l) {
    l->state = LAUNCHER_CLOSED;
    l->selected_option = 0;
}

static const char *launcher_option_name(int option) {
    switch (option) {
        case 1: return "Terminal";
        case 2: return "File Manager";
        case 3: return "Settings";
        default: return "Unknown";
    }
}

int main(void)
{
    printf("=== Launcher Tests ===\n\n");

    launcher_t l = { .state = LAUNCHER_CLOSED, .selected_option = 0, .num_options = 3 };

    printf("--- Initial state ---\n");
    TEST_ASSERT(l.state == LAUNCHER_CLOSED, "Initial state is CLOSED");
    TEST_ASSERT(l.selected_option == 0, "No option selected initially");

    printf("\n--- Open launcher ---\n");
    launcher_open(&l);
    TEST_ASSERT(l.state == LAUNCHER_OPEN, "State is OPEN after launcher_open");
    TEST_ASSERT(l.selected_option == 0, "No option selected when opened");

    printf("\n--- Select option 1 (Terminal) ---\n");
    launcher_select(&l, 1);
    TEST_ASSERT(l.state == LAUNCHER_SELECTING, "State is SELECTING after select");
    TEST_ASSERT(l.selected_option == 1, "Option 1 selected");
    TEST_ASSERT(strcmp(launcher_option_name(1), "Terminal") == 0, "Option 1 is Terminal");

    printf("\n--- Close after select ---\n");
    launcher_close(&l);
    TEST_ASSERT(l.state == LAUNCHER_CLOSED, "State is CLOSED after close");
    TEST_ASSERT(l.selected_option == 0, "Selection cleared after close");

    printf("\n--- Select option 2 (File Manager) ---\n");
    launcher_open(&l);
    launcher_select(&l, 2);
    TEST_ASSERT(l.selected_option == 2, "Option 2 selected");
    TEST_ASSERT(strcmp(launcher_option_name(2), "File Manager") == 0, "Option 2 is File Manager");

    printf("\n--- Select option 3 (Settings) ---\n");
    launcher_close(&l);
    launcher_open(&l);
    launcher_select(&l, 3);
    TEST_ASSERT(l.selected_option == 3, "Option 3 selected");
    TEST_ASSERT(strcmp(launcher_option_name(3), "Settings") == 0, "Option 3 is Settings");

    printf("\n--- Invalid options ---\n");
    launcher_close(&l);
    launcher_open(&l);
    launcher_select(&l, 0);
    TEST_ASSERT(l.selected_option == 0, "Option 0 rejected (too low)");
    launcher_select(&l, 4);
    TEST_ASSERT(l.selected_option == 0, "Option 4 rejected (too high)");
    launcher_select(&l, -1);
    TEST_ASSERT(l.selected_option == 0, "Option -1 rejected");

    printf("\n--- Select when closed (no-op) ---\n");
    launcher_close(&l);
    launcher_select(&l, 1);
    TEST_ASSERT(l.selected_option == 0, "Select when closed is no-op");
    TEST_ASSERT(l.state == LAUNCHER_CLOSED, "State stays CLOSED");

    printf("\n--- Open-close-open cycle ---\n");
    launcher_open(&l);
    launcher_close(&l);
    launcher_open(&l);
    TEST_ASSERT(l.state == LAUNCHER_OPEN, "Re-opened after close cycle");
    launcher_select(&l, 1);
    TEST_ASSERT(l.selected_option == 1, "Select works after re-open");

    printf("\n--- Option names ---\n");
    TEST_ASSERT(strcmp(launcher_option_name(1), "Terminal") == 0, "Name 1 = Terminal");
    TEST_ASSERT(strcmp(launcher_option_name(2), "File Manager") == 0, "Name 2 = File Manager");
    TEST_ASSERT(strcmp(launcher_option_name(3), "Settings") == 0, "Name 3 = Settings");
    TEST_ASSERT(strcmp(launcher_option_name(0), "Unknown") == 0, "Name 0 = Unknown");
    TEST_ASSERT(strcmp(launcher_option_name(99), "Unknown") == 0, "Name 99 = Unknown");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}