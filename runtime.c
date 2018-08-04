
#include <string.h>
#include "runtime.h"

#define CURSOR_GLYPH 127
#define SCREEN_HEIGHT 24
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)
#define MIXED_TEXT_HEIGHT 4
#define MIXED_GRAPHICS_HEIGHT (SCREEN_HEIGHT - MIXED_TEXT_HEIGHT)
#define CLEAR_CHAR (' ' | 0x80)

// Location of cursor in logical screen space.
uint16_t g_cursor_x = 0;
uint16_t g_cursor_y = 0;
// Whether the cursor is being displayed.
uint16_t g_showing_cursor = 0;
// Character at the cursor location.
uint8_t g_cursor_ch = 0;

// Whether in low-res graphics mode.
uint8_t g_gr_mode = 0;

// 4-bit low-res color.
uint8_t g_gr_color_high = 0; // High nybble.
uint8_t g_gr_color_low = 0;  // Low nybble.

// List of variable names, two bytes each, in the same order they are
// in the zero page (starting at FIRST_VARIABLE). Two nuls means an unused
// slot. One-letter variable names have a nul for the second character.
uint8_t g_variable_names[MAX_VARIABLES*2];

/**
 * Clear out the values of all variables.
 */
void clear_variable_values(void) {
    memset((void *) FIRST_VARIABLE, 0, MAX_VARIABLES*2);
}

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
    if (g_gr_mode) {
        int i;

        for (i = MIXED_GRAPHICS_HEIGHT; i < SCREEN_HEIGHT; i++) {
            memset(screen_pos(0, i), CLEAR_CHAR, SCREEN_WIDTH);
        }

        move_cursor(0, MIXED_GRAPHICS_HEIGHT);
    } else {
        memset(TEXT_PAGE1_BASE, CLEAR_CHAR, SCREEN_STRIDE*8);
        move_cursor(0, 0);
    }
}

/**
 * Screen the screen up one line, blanking out the bottom
 * row. Does not affect the cursor.
 */
static void scroll_up(void) {
    int i;
    int first_line = g_gr_mode ? MIXED_GRAPHICS_HEIGHT : 0;
    uint8_t *previous_line = 0;

    for (i = first_line; i < SCREEN_HEIGHT; i++) {
        uint8_t *this_line = screen_pos(0, i);
        if (i > first_line) {
            memmove(previous_line, this_line, SCREEN_WIDTH);
        }
        previous_line = this_line;
    }

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

/**
 * Switch to graphics mode.
 */
void gr_statement(void) {
    int i;
    uint8_t *p = (uint8_t *) 49235U;

    hide_cursor();

    // Mixed text and lo-res graphics mode.
    *p = 0;

    // Clear the graphics area.
    for (i = 0; i < MIXED_GRAPHICS_HEIGHT; i++) {
        memset(screen_pos(0, i), 0, SCREEN_WIDTH);
    }

    // Move the cursor to the text window.
    if (g_cursor_y < MIXED_GRAPHICS_HEIGHT) {
        move_cursor(0, MIXED_GRAPHICS_HEIGHT);
    }

    g_gr_mode = 1;
}

/**
 * Switch to text mode.
 */
void text_statement(void) {
    uint8_t *p = (uint8_t *) 49233U;

    hide_cursor();

    // Mixed text and lo-res graphics mode.
    *p = 0;

    g_gr_mode = 0;
}

/**
 * Set the low-res color.
 */
void color_statement(uint16_t color) {
    g_gr_color_high = (uint8_t) ((color << 4) & 0xF0);
    g_gr_color_low = (uint8_t) (color & 0x0F);
}

/**
 * Plot a pixel in low-res graphics mode.
 */
void plot_statement(uint16_t x, uint16_t y) {
    uint8_t *pos = screen_pos(x, y >> 1);

    if ((y & 1) == 0) {
        // Even, bottom pixel.
        *pos = (*pos & 0xF0) | g_gr_color_low;
    } else {
        // Odd, top pixel.
        *pos = (*pos & 0x0F) | g_gr_color_high;
    }
}
