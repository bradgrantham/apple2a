char *title = "Apple IIa";
unsigned char title_length = 9;

volatile unsigned char *text_page1_base = (unsigned char *)0x400;
volatile unsigned char *text_page2_base = (unsigned char *)0x800;

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
    volatile unsigned char *p = text_page1_base;
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

    volatile unsigned char *loc = text_page1_base + offset;

    int i;

    home();

    // Title.
    for(i = 0; i < title_length; i++) {
        loc[i] = title[i] | 0x80;
    }

    // Prompt.
    loc = text_page1_base + (40 + 40 + 48)*2;
    *loc++ = ']' | 0x80;

    // Cursor.
    while(1) {
        *loc = 127 | 0x80;
        delay(2500);
        *loc = ' ' | 0x80;
        delay(2500);
    }

    return 0;
}
