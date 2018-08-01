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
unsigned char input_buffer[40];
int input_buffer_length = 0;

// Compiled binary.
char binary[128];
int binary_length = 0;
void (*binary_function)() = (void (*)()) binary;

/**
 * Delay for a count of "t". 8000 is about one second.
 */
static void delay(int t) {
    while (t >= 0) {
        t--;
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

    move_cursor(0, 0);
}

/**
 * Print a string at the cursor.
 */
static void print(unsigned char *s) {
    volatile unsigned char *loc = cursor_pos();

    while (*s != '\0') {
        if (*s == '\n') {
            // TODO: Scroll.
            move_cursor(0, cursor_y + 1);
        } else {
            *loc = *s | 0x80;
            move_cursor(cursor_x + 1, cursor_y);
        }
        loc = cursor_pos();
        s += 1;
    }
}

static void print_statement() {
    print("Hello world!\n");
}

/**
 * If a starts with string b, returns the position in a after b. Else returns null.
 */
static unsigned char *skip_over(unsigned char *a, unsigned char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            // Doesn't start with b.
            return 0;
        }

        a += 1;
        b += 1;
    }

    // See if we're at the end of b.
    return *b == '\0' ? a : 0;
}

/**
 * Display a syntax error message.
 */
static void syntax_error() {
    print("\n?SYNTAX ERROR");
    // No linefeed, assume prompt will do it.
}

/**
 * Add a function call to the binary buffer.
 */
static void add_call(void (*function)(void)) {
    unsigned int addr = (int) function;

    binary[binary_length++] = 0x20;  // JSR
    binary[binary_length++] = addr & 0xFF;
    binary[binary_length++] = addr >> 8;
}

/**
 * Add a function return to the binary buffer.
 */
static void add_return() {
    binary[binary_length++] = 0x60;  // RTS
}

/**
 * Advance s over whitespace, which is just a space, returning
 * the new pointer.
 */
static unsigned char *skip_whitespace(unsigned char *s) {
    while (*s == ' ') {
        s += 1;
    }

    return s;
}

/**
 * Process the user's line of input, possibly compiling the code.
 * and executing it.
 */
static void process_input_buffer() {
    unsigned char *s; // Where we are in the buffer.
    unsigned char *after; // After skipping a token.
    char done;

    input_buffer[input_buffer_length] = '\0';
    s = input_buffer;

    // Compile the line of BASIC.
    binary_length = 0;

    do {
        char error = 0;

        // Default to being done after one command.
        done = 1;

        s = skip_whitespace(s);
        if (*s == '\0' || *s == ':') {
            // Empty statement.
        } else if ((after = skip_over(s, "HOME")) != 0) {
            s = after;
            add_call(home);
        } else if ((after = skip_over(s, "PRINT")) != 0) {
            s = after;

            // TODO: Parse expression.
            add_call(print_statement);
        } else {
            error = 1;
        }

        // Now we're at the end of our statement.
        if (!error) {
            s = skip_whitespace(s);
            if (*s == ':') {
                // Skip colon.
                s += 1;

                // Next statement.
                done = 0;
            } else if (*s != '\0') {
                // Junk at the end of the statement.
                error = 1;
            }
        }

        if (error) {
            add_call(syntax_error);
        }
    } while (!done);

    // Return from function.
    add_return();

    if (binary_length > sizeof(binary)) {
        // TODO: Check while adding bytes, not at the end.
        print("\n?Binary length exceeded");
    } else {
        // Call it.
        binary_function();
    }
}

int main(void)
{
    volatile unsigned char *loc;
    int i;

    home();

    /*
    // Display the character set.
    for (i = 0; i < 256; i++) {
        // Fails with: unhandled instruction B2
        move_cursor(i % 16, i >> 4);
        // Works.
        // move_cursor(i & 0x0F, i >> 4);
        loc = cursor_pos();
        *loc = i;
    }
    while(1);
    */


    // Title.
    move_cursor((40 - title_length) / 2, 0);
    print(title);

    // Prompt.
    print("\n\n]");

    // Keyboard input.
    i = 0;
    input_buffer_length = 0;
    show_cursor();
    while(1) {
        // Blink cursor.
        i += 1;
        if (i == 3000) {
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
                    if (input_buffer_length > 0) {
                        move_cursor(cursor_x - 1, cursor_y);
                        input_buffer_length -= 1;
                    }
                } else if (key == 13) {
                    // Return.
                    move_cursor(0, cursor_y + 1);

                    process_input_buffer();

                    print("\n]");
                    input_buffer_length = 0;
                } else {
                    if (input_buffer_length < sizeof(input_buffer) - 1) {
                        volatile unsigned char *loc = cursor_pos();
                        *loc = key | 0x80;
                        move_cursor(cursor_x + 1, cursor_y);

                        input_buffer[input_buffer_length++] = key;
                    }
                }
            }

            show_cursor();
        }
    }

    return 0;
}
