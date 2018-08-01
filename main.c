#include "platform.h"

char *title = "Apple IIa";
unsigned char title_length = 9;

#define CURSOR_GLYPH 127
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)

#define T_HOME 0x80
#define T_PRINT 0x81

// List of tokens. The token value is the index plus 0x80.
static unsigned char *TOKEN[] = {
    "HOME",
    "PRINT",
};
static int TOKEN_COUNT = sizeof(TOKEN)/sizeof(TOKEN[0]);

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
char compiled[128];
int compiled_length = 0;
void (*compiled_function)() = (void (*)()) compiled;

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
 * Prints the character and advances the cursor. Handles newlines.
 */
static void print_char(unsigned char c) {
    volatile unsigned char *loc = cursor_pos();

    if (c == '\n') {
        // TODO: Scroll.
        move_cursor(0, cursor_y + 1);
    } else {
        // Print character.
        *loc = c | 0x80;
        move_cursor(cursor_x + 1, cursor_y);
    }
}

/**
 * Print a string at the cursor.
 */
static void print(unsigned char *s) {
    while (*s != '\0') {
        print_char(*s++);
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
 * Add a function call to the compiled buffer.
 */
static void add_call(void (*function)(void)) {
    unsigned int addr = (int) function;

    compiled[compiled_length++] = 0x20;  // JSR
    compiled[compiled_length++] = addr & 0xFF;
    compiled[compiled_length++] = addr >> 8;
}

/**
 * Add a function return to the compiled buffer.
 */
static void add_return() {
    compiled[compiled_length++] = 0x60;  // RTS
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
 * Tokenize a string in place. Returns (and removes) any line number, or 0xFFFF
 * if there's none. The new line will be terminated by three nuls.
 */
static unsigned int tokenize(unsigned char *s) {
    unsigned char *t = s; // Tokenized version.
    int line_number;

    // Parse optional line number.
    if (*s >= '0' && *s <= '9') {
        line_number = 0;

        while (*s >= '0' && *s <= '9') {
            line_number = line_number*10 + (*s - '0');
            s += 1;
        }
    } else {
        line_number = 0xFFFF;
    }

    // Convert tokens.
    while (*s != '\0') {
        if (*s == ' ') {
            // Skip spaces.
            s++;
        } else {
            int i;
            unsigned char *skipped = 0;

            for (i = 0; i < TOKEN_COUNT; i++) {
                skipped = skip_over(s, TOKEN[i]);
                if (skipped != 0) {
                    // Record token.
                    *t++ = 0x80 + i;
                    s = skipped;
                    break;
                }
            }

            if (skipped == 0) {
                // Didn't find a token, just copy text.
                *t++ = *s++;
            }
        }
    }

    // End with three nuls. The first is the end of line.
    // The next two is the address of the next line, which is "none".
    *t++ = '\0';
    *t++ = '\0';
    *t++ = '\0';

    return line_number;
}

/**
 * Print the tokenized string, with tokens displayed as their full text.
 * Prints a line number first if it's not 0xFFFF. Prints a newline at the end.
 */
static void print_detokenized(unsigned int line_number, unsigned char *s) {
    while (*s != '\0') {
        if (*s >= 0x80) {
            print(TOKEN[*s - 0x80]);
        } else {
            print_char(*s);
        }

        s += 1;
    }

    print_char('\n');
}

/**
 * Process the user's line of input, possibly compiling the code.
 * and executing it.
 */
static void process_input_buffer() {
    unsigned char *s; // Where we are in the buffer.
    char done;
    unsigned int line_number;

    input_buffer[input_buffer_length] = '\0';

    // Tokenize in-place.
    line_number = tokenize(input_buffer);

    s = input_buffer;

    // Compile the line of BASIC.
    compiled_length = 0;

    do {
        char error = 0;

        // Default to being done after one statement.
        done = 1;

        if (*s == '\0' || *s == ':') {
            // Empty statement.
        } else if (*s == T_HOME) {
            s += 1;
            add_call(home);
        } else if (*s == T_PRINT) {
            s += 1;

            // TODO: Parse expression.
            add_call(print_statement);
        } else {
            error = 1;
        }

        // Now we're at the end of our statement.
        if (!error) {
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

    if (compiled_length > sizeof(compiled)) {
        // TODO: Check while adding bytes, not at the end.
        print("\n?Binary length exceeded");
    } else {
        // Call it.
        compiled_function();
    }
}

int main(void)
{
    int i;

    home();

    /*
    // Display the character set.
    for (i = 0; i < 256; i++) {
        volatile unsigned char *loc;
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
