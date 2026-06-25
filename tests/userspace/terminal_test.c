/**
 * @file terminal_test.c
 * @brief Tests for terminal character grid + cursor movement
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int g_pass = 0, g_fail = 0;
#define TEST_ASSERT(c,m) do{if(c){g_pass++;printf("  PASS: %s\n",m);}else{g_fail++;printf("  FAIL: %s\n",m);}}while(0)

#define COLS 100
#define ROWS 37

static char grid[ROWS][COLS];
static int cursor_col = 0, cursor_row = 0;

static void grid_init(void)
{
    memset(grid, 0, sizeof(grid));
    cursor_col = 0;
    cursor_row = 0;
}

static void put_char(char c)
{
    if (c == '\n') { cursor_col = 0; cursor_row++; return; }
    if (cursor_col >= COLS) { cursor_col = 0; cursor_row++; }
    if (cursor_row >= ROWS) {
        memmove(grid, grid[1], (ROWS-1)*COLS);
        memset(grid[ROWS-1], 0, COLS);
        cursor_row = ROWS-1;
    }
    grid[cursor_row][cursor_col] = c;
    cursor_col++;
}

static void term_print(const char *s)
{
    while (*s) put_char(*s++);
}

int main(void)
{
    printf("=== Terminal Grid Tests ===\n\n");

    printf("--- Initial state ---\n");
    grid_init();
    TEST_ASSERT(cursor_col == 0, "Cursor col starts at 0");
    TEST_ASSERT(cursor_row == 0, "Cursor row starts at 0");
    TEST_ASSERT(grid[0][0] == 0, "Grid is empty initially");

    printf("\n--- Character placement ---\n");
    grid_init();
    put_char('A');
    TEST_ASSERT(grid[0][0] == 'A', "First char at (0,0)");
    TEST_ASSERT(cursor_col == 1, "Cursor advances to col 1");
    put_char('B');
    TEST_ASSERT(grid[0][1] == 'B', "Second char at (0,1)");
    TEST_ASSERT(cursor_col == 2, "Cursor at col 2");

    printf("\n--- Newline handling ---\n");
    grid_init();
    put_char('X');
    put_char('\n');
    TEST_ASSERT(cursor_col == 0, "Col resets to 0 after newline");
    TEST_ASSERT(cursor_row == 1, "Row advances to 1 after newline");
    put_char('Y');
    TEST_ASSERT(grid[1][0] == 'Y', "Char after newline at (1,0)");

    printf("\n--- Line wrap ---\n");
    grid_init();
    for (int i = 0; i < COLS; i++) put_char('a' + (i % 26));
    TEST_ASSERT(cursor_col == 0, "Col wraps to 0 after filling row");
    TEST_ASSERT(cursor_row == 1, "Row advances after line wrap");
    TEST_ASSERT(grid[0][0] == 'a', "First char of wrapped line correct");

    printf("\n--- String printing ---\n");
    grid_init();
    term_print("Hello\nWorld");
    TEST_ASSERT(grid[0][0] == 'H', "String starts with H");
    TEST_ASSERT(grid[0][4] == 'o', "String ends with o on row 0");
    TEST_ASSERT(grid[1][0] == 'W', "Newline starts W on row 1");
    TEST_ASSERT(grid[1][4] == 'd', "World ends with d");

    printf("\n--- Clear screen ---\n");
    grid_init();
    term_print("Test data here");
    grid_init();
    TEST_ASSERT(grid[0][0] == 0, "Grid cleared after init");
    TEST_ASSERT(cursor_row == 0, "Cursor reset after clear");

    printf("\n--- Multiple lines ---\n");
    grid_init();
    term_print("L1\nL2\nL3\nL4");
    TEST_ASSERT(grid[0][0] == 'L', "Line 1 at row 0");
    TEST_ASSERT(grid[1][0] == 'L', "Line 2 at row 1");
    TEST_ASSERT(grid[2][0] == 'L', "Line 3 at row 2");
    TEST_ASSERT(grid[3][0] == 'L', "Line 4 at row 3");
    TEST_ASSERT(cursor_row == 3, "Cursor at row 3 after 4 lines");

    printf("\n--- Scroll behavior ---\n");
    grid_init();
    for (int i = 0; i < ROWS + 5; i++)
    {
        char buf[8]; snprintf(buf, sizeof(buf), "L%d", i);
        term_print(buf); put_char('\n');
    }
    TEST_ASSERT(cursor_row == ROWS - 1, "Cursor clamped to last row after scroll");
    TEST_ASSERT(grid[0][0] != 'L' || grid[0][1] != '0', "First line scrolled away");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}