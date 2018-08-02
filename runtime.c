
#include "runtime.h"

#define CURSOR_GLYPH 127
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)

// Location of cursor in logical screen space.
uint16_t g_cursor_x = 0;
uint16_t g_cursor_y = 0;
// Whether the cursor is being displayed.
uint16_t g_showing_cursor = 0;
// Character at the cursor location.
uint8_t g_cursor_ch = 0;

/**
 * Return the memory location of the cursor.
 */
volatile uint8_t *cursor_pos(void) {
    int16_t block = g_cursor_y >> 3;
    int16_t line = g_cursor_y & 0x07;

    return TEXT_PAGE1_BASE + line*SCREEN_STRIDE + block*SCREEN_WIDTH + g_cursor_x;
}

/**
 * Shows the cursor. Safe to call if it's already showing.
 */
void show_cursor(void) {
    if (!g_showing_cursor) {
        volatile uint8_t *pos = cursor_pos();
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
        volatile uint8_t *pos = cursor_pos();
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
 * Clear the screen with non-reversed spaces.
 */
void home(void) {
    volatile uint8_t *p = TEXT_PAGE1_BASE;
    uint8_t ch = ' ' | 0x80;
    int16_t i;

    // TODO: Could write these as words, not chars.
    for (i = SCREEN_STRIDE*8; i >= 0; i--) {
        *p++ = ch;
    }

    move_cursor(0, 0);
}

/**
 * Prints the character and advances the cursor. Handles newlines.
 */
void print_char(uint8_t c) {
    volatile uint8_t *loc = cursor_pos();

    if (c == '\n') {
        // TODO: Scroll.
        move_cursor(0, g_cursor_y + 1);
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
