#include "platform.h"

char *title = "Apple IIa";
unsigned char title_length = 9;

#define CURSOR_GLYPH 127
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)

// Location of cursor in logical screen space.
unsigned int cursor_x = 0;
unsigned int cursor_y = 0;
// Whether the cursor is being displayed.
unsigned int showing_cursor = 0;
// Character at the cursor location.
unsigned char cursor_ch = 0;

/**
 * Delay for a count of "t". 8000 is about one second.
 */
static void delay(int t) {
    while (t >= 0) {
        t--;
    }
}

/**
 * Clear the screen with non-reversed spaces.
 */
static void home() {
    volatile unsigned char *p = TEXT_PAGE1_BASE;
    unsigned char ch = ' ' | 0x80;
    int i;

    // TODO: Could write these as words, not chars.
    for (i = SCREEN_STRIDE*8; i >= 0; i--) {
        *p++ = ch;
    }
}

/**
 * Return the memory location of the cursor.
 */
static volatile unsigned char *cursor_pos() {
    int block = cursor_y >> 3;
    int line = cursor_y & 0x07;

    return TEXT_PAGE1_BASE + line*SCREEN_STRIDE + block*SCREEN_WIDTH + cursor_x;
}

/**
 * Shows the cursor. Safe to call if it's already showing.
 */
static void show_cursor() {
    if (!showing_cursor) {
        volatile unsigned char *pos = cursor_pos();
        cursor_ch = *pos;
        *pos = CURSOR_GLYPH | 0x80;
        showing_cursor = 1;
    }
}

/**
 * Hides the cursor. Safe to call if it's not already shown.
 */
static void hide_cursor() {
    if (showing_cursor) {
        volatile unsigned char *pos = cursor_pos();
        *pos = cursor_ch;
        showing_cursor = 0;
    }
}

/**
 * Moves the cursor to the specified location, where X
 * is 0 to 39 inclusive, Y is 0 to 23 inclusive.
 */
static void move_cursor(int x, int y) {
    hide_cursor();
    cursor_x = x;
    cursor_y = y;
}

int main(void)
{
    int offset = (40 - title_length) / 2;

    volatile unsigned char *loc = TEXT_PAGE1_BASE + offset;

    int i;

    home();

    // Title.
    for(i = 0; i < title_length; i++) {
        loc[i] = title[i] | 0x80;
    }

    // Prompt.
    move_cursor(0, 2);
    loc = cursor_pos();
    *loc = ']' | 0x80;
    move_cursor(cursor_x + 1, cursor_y);

    // Keyboard input.
    i = 0;
    show_cursor();
    while(1) {
        // Blink cursor.
        i += 1;
        if (i == 2000) {
            if (showing_cursor) {
                hide_cursor();
            } else {
                show_cursor();
            }
            i = 0;
        }

        if(keyboard_test()) {
            hide_cursor();

            while(keyboard_test()) {
                unsigned char key;

                key = keyboard_get();
                if (key == 8) {
                    // Backspace.
                    if (cursor_x > 1) {
                        move_cursor(cursor_x - 1, cursor_y);
                    }
                } else if (key == 13) {
                    // Return.
                    move_cursor(0, cursor_y + 1);
                    loc = cursor_pos();
                    *loc = ']' | 0x80;
                    move_cursor(cursor_x + 1, cursor_y);
                } else {
                    volatile unsigned char *loc = cursor_pos();
                    *loc = key | 0x80;
                    move_cursor(cursor_x + 1, cursor_y);
                }
            }

            show_cursor();
        }
    }

    return 0;
}
