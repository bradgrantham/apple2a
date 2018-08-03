
#include <string.h>
#include "runtime.h"

#define CURSOR_GLYPH 127
#define SCREEN_HEIGHT 24
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)
#define CLEAR_CHAR (' ' | 0x80)

// Location of cursor in logical screen space.
uint16_t g_cursor_x = 0;
uint16_t g_cursor_y = 0;
// Whether the cursor is being displayed.
uint16_t g_showing_cursor = 0;
// Character at the cursor location.
uint8_t g_cursor_ch = 0;

/**
 * Return the memory location of the zero-based (x,y) position on the screen.
 */
static uint8_t *screen_pos(uint16_t x, uint16_t y) {
    int16_t block = y >> 3;
    int16_t line = y & 0x07;

    return TEXT_PAGE1_BASE + line*SCREEN_STRIDE + block*SCREEN_WIDTH + x;
}

/**
 * Return the memory location of the cursor.
 */
uint8_t *cursor_pos(void) {
    return screen_pos(g_cursor_x, g_cursor_y);
}

/**
 * Shows the cursor. Safe to call if it's already showing.
 */
void show_cursor(void) {
    if (!g_showing_cursor) {
        uint8_t *pos = cursor_pos();
        g_cursor_ch = *pos;
        *pos = CURSOR_GLYPH | 0x80;
        g_showing_cursor = 1;
    }
}

/**
 * Hides the cursor. Safe to call if it's not already shown.
 */
void hide_cursor(void) {
    if (g_showing_cursor) {
        uint8_t *pos = cursor_pos();
        *pos = g_cursor_ch;
        g_showing_cursor = 0;
    }
}

/**
 * Moves the cursor to the specified location, where X
 * is 0 to 39 inclusive, Y is 0 to 23 inclusive.
 */
void move_cursor(int16_t x, int16_t y) {
    hide_cursor();
    g_cursor_x = x;
    g_cursor_y = y;
}

/**
 * Blanks out the rest of the line, from the cursor (inclusive) on.
 * Does not move the cursor.
 */
void clear_to_eol(void) {
    uint8_t *pos = cursor_pos();

    hide_cursor();
    memset(pos, CLEAR_CHAR, SCREEN_WIDTH - g_cursor_x);
}

/**
 * Clear the screen with non-reversed spaces.
 */
void home(void) {
    memset(TEXT_PAGE1_BASE, CLEAR_CHAR, SCREEN_STRIDE*8);
    move_cursor(0, 0);
}

/**
 * Screen the screen up one line, blanking out the bottom
 * row. Does not affect the cursor.
 */
static void scroll_up(void) {
    int i;
    uint8_t *previous_line = 0;

    for (i = 0; i < SCREEN_HEIGHT; i++) {
        uint8_t *this_line = screen_pos(0, i);
        if (i > 0) {
            memmove(previous_line, this_line, SCREEN_WIDTH);
        }
        previous_line = this_line;
    }

    // This is provided by cc65:
    memset(previous_line, CLEAR_CHAR, SCREEN_WIDTH);
}

/**
 * Prints the character and advances the cursor. Handles newlines.
 */
void print_char(uint8_t c) {
    uint8_t *loc = cursor_pos();

    if (c == '\n') {
        if (g_cursor_y == SCREEN_HEIGHT - 1) {
            // Scroll.
            hide_cursor();
            scroll_up();
            move_cursor(0, g_cursor_y);
        } else {
            move_cursor(0, g_cursor_y + 1);
        }
    } else {
        // Print character.
        *loc = c | 0x80;
        move_cursor(g_cursor_x + 1, g_cursor_y);
    }
}

/**
 * Print a string at the cursor.
 */
void print(uint8_t *s) {
    while (*s != '\0') {
        print_char(*s++);
    }
}

/**
 * Print an unsigned integer.
 */
void print_int(uint16_t i) {
    // Is this the best way to do this? I've seen it done backwards, where
    // digits are added to a buffer least significant digit first, then reversed,
    // but this seems faster.
    char printed = 0;
    if (i >= 10000) {
        int16_t r = i / 10000;
        print_char('0' + r);
        i -= r*10000;
        printed = 1;
    }
    if (i >= 1000 || printed) {
        int16_t r = i / 1000;
        print_char('0' + r);
        i -= r*1000;
        printed = 1;
    }
    if (i >= 100 || printed) {
        int16_t r = i / 100;
        print_char('0' + r);
        i -= r*100;
        printed = 1;
    }
    if (i >= 10 || printed) {
        int16_t r = i / 10;
        print_char('0' + r);
        i -= r*10;
    }
    print_char('0' + i);
}

/**
 * Print a single newline.
 */
void print_newline(void) {
    print_char('\n');
}

/**
 * Display a syntax error message.
 */
void syntax_error(void) {
    print("\n?SYNTAX ERROR");
    // No linefeed, assume prompt will do it.
}

/**
 * Display a syntax error message for stored program.
 */
void syntax_error_in_line(uint16_t line_number) {
    print("\n?SYNTAX ERROR IN ");
    print_int(line_number);

    // No linefeed, assume prompt will do it.
}
