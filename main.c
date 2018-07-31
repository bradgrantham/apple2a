#include "platform.h"

char *title = "Apple IIa";
unsigned char title_length = 9;

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
    for (i = (40 + 40 + 48)*8; i >= 0; i--) {
        *p++ = ch;
    }
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
    loc = TEXT_PAGE1_BASE + (40 + 40 + 48)*2;
    *loc++ = ']' | 0x80;

    // Cursor.
    i = 1;
    while(1) {
        *loc = 127 | 0x80;
        delay(2500);
        *loc = ' ' | 0x80;
        delay(2500);

        while(keyboard_test()) {
            unsigned char key;

            key = keyboard_get();
            loc[i++] = key | 0x80;
        }
    }

    return 0;
}
