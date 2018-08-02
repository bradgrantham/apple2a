#include "platform.h"

uint8_t *title = "Apple IIa";
uint8_t title_length = 9;

#define CURSOR_GLYPH 127
#define SCREEN_WIDTH 40
#define SCREEN_STRIDE (3*SCREEN_WIDTH + 8)

#define T_HOME 0x80
#define T_PRINT 0x81
#define T_LIST 0x82

// List of tokens. The token value is the index plus 0x80.
static uint8_t *TOKEN[] = {
    "HOME",
    "PRINT",
    "LIST",
};
static int16_t TOKEN_COUNT = sizeof(TOKEN)/sizeof(TOKEN[0]);

// Location of cursor in logical screen space.
uint16_t g_cursor_x = 0;
uint16_t g_cursor_y = 0;
// Whether the cursor is being displayed.
uint16_t g_showing_cursor = 0;
// Character at the cursor location.
uint8_t g_cursor_ch = 0;
uint8_t g_input_buffer[40];
int16_t g_input_buffer_length = 0;

// Compiled binary.
uint8_t g_compiled[128];
int16_t g_compiled_length = 0;
void (*g_compiled_function)() = (void (*)()) g_compiled;

// Stored program. Each line is:
// - Two bytes for pointer to next line (or zero if none).
// - Two bytes for line number.
// - Program line.
// - Nul.
uint8_t g_program[1024];

/**
 * Return the memory location of the cursor.
 */
static volatile uint8_t *cursor_pos() {
    int16_t block = g_cursor_y >> 3;
    int16_t line = g_cursor_y & 0x07;

    return TEXT_PAGE1_BASE + line*SCREEN_STRIDE + block*SCREEN_WIDTH + g_cursor_x;
}

/**
 * Shows the cursor. Safe to call if it's already showing.
 */
static void show_cursor() {
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
static void hide_cursor() {
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
static void move_cursor(int16_t x, int16_t y) {
    hide_cursor();
    g_cursor_x = x;
    g_cursor_y = y;
}

/**
 * Clear the screen with non-reversed spaces.
 */
static void home() {
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
static void print_char(uint8_t c) {
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
static void print(uint8_t *s) {
    while (*s != '\0') {
        print_char(*s++);
    }
}

/**
 * Print an unsigned integer.
 */
static void print_int(uint16_t i) {
    // Is this the best way to do this? I've seen it done backwards, where
    // digits are added to a buffer least significant digit first, then reversed,
    // but this seems faster.
    if (i >= 10000) {
        int16_t r = i / 10000;
        print_char('0' + r);
        i -= r*10000;
    }
    if (i >= 1000) {
        int16_t r = i / 1000;
        print_char('0' + r);
        i -= r*1000;
    }
    if (i >= 100) {
        int16_t r = i / 100;
        print_char('0' + r);
        i -= r*100;
    }
    if (i >= 10) {
        int16_t r = i / 10;
        print_char('0' + r);
        i -= r*10;
    }
    print_char('0' + i);
}

/**
 * Copy a memory buffer. Source and destination may overlap.
 */
static void memmove(uint8_t *dest, uint8_t *src, uint16_t count) {
    // See if we overlap.
    if (dest > src && dest < src + count) {
        // Overlapping with src before dest, we have to copy backward.
        dest += count;
        src += count;
        while (count-- > 0) {
            *--dest = *--src;
        }
    } else {
        // No overlap, or dest before src, which is fine.
        while (count-- > 0) {
            *dest++ = *src++;
        }
    }
}

/**
 * Get the length of a nul-terminated string.
 */
static int16_t strlen(uint8_t *s) {
    uint8_t *original = s;

    while (*s != '\0') {
        s += 1;
    }

    return s - original;
}

/**
 * Print the tokenized string, with tokens displayed as their full text.
 * Prints a newline at the end.
 */
static void print_detokenized(uint8_t *s) {
    while (*s != '\0') {
        if (*s >= 0x80) {
            print_char(' ');
            print(TOKEN[*s - 0x80]);
            print_char(' ');
        } else {
            print_char(*s);
        }

        s += 1;
    }

    print_char('\n');
}

/**
 * Get the pointer to the next line in the stored program. Returns 0
 * if we're at the end.
 */
static uint8_t *get_next_line(uint8_t *line) {
    return *((uint8_t **) line);
}

/**
 * Get the line number of a stored program line.
 */
static uint16_t get_line_number(uint8_t *line) {
    return *((uint16_t *) (line + 2));
}

/**
 * Return a pointer to the end of the program. This is one byte PAST the
 * last bytes in the program, which are two nuls. The "line" parameter is
 * an optional starting point, to use as an optimization instead of starting
 * from the beginning.
 */
static uint8_t *get_end_of_program(uint8_t *line) {
    uint8_t *next_line;

    if (line == 0) {
        // Start at the beginning if not specified.
        line = g_program;
    }

    while ((next_line = get_next_line(line)) != 0) {
        line = next_line;
    }

    // Skip the null "next" pointer.
    return line + 2;
}

static void print_statement() {
    print("Hello world!\n");
}

/**
 * List the stored program.
 */
static void list_statement() {
    uint8_t *line = g_program;
    uint8_t *next_line;

    while ((next_line = get_next_line(line)) != 0) {
        print_int(get_line_number(line));
        print_char(' ');
        print_detokenized(line + 4);

        line = next_line;
    }
}

/**
 * If a starts with string b, returns the position in a after b. Else returns null.
 */
static uint8_t *skip_over(uint8_t *a, uint8_t *b) {
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
    uint16_t addr = (int16_t) function;

    g_compiled[g_compiled_length++] = 0x20;  // JSR
    g_compiled[g_compiled_length++] = addr & 0xFF;
    g_compiled[g_compiled_length++] = addr >> 8;
}

/**
 * Add a function return to the compiled buffer.
 */
static void add_return() {
    g_compiled[g_compiled_length++] = 0x60;  // RTS
}

/**
 * Advance s over whitespace, which is just a space, returning
 * the new pointer.
 */
static uint8_t *skip_whitespace(uint8_t *s) {
    while (*s == ' ') {
        s += 1;
    }

    return s;
}

/**
 * Tokenize a string in place. Returns (and removes) any line number, or 0xFFFF
 * if there's none.
 */
static uint16_t tokenize(uint8_t *s) {
    uint8_t *t = s; // Tokenized version.
    int16_t line_number;

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
            int16_t i;
            uint8_t *skipped = 0;

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

    // Terminate string.
    *t++ = '\0';

    return line_number;
}

/**
 * Find the stored program line with the given line number. If the line does
 * not exist, returns a pointer to the location where it should be inserted.
 */
static uint8_t *find_line(uint16_t line_number) {
    uint8_t *line = g_program;
    uint8_t *next_line;

    while ((next_line = get_next_line(line)) != 0) {
        // See if we hit it or just blew past it.
        if (get_line_number(line) >= line_number) {
            break;
        }

        line = next_line;
    }

    return line;
}

/**
 * Process the user's line of input, possibly compiling the code.
 * and executing it.
 */
static void process_input_buffer() {
    uint8_t *s; // Where we are in the buffer.
    int8_t done;
    uint16_t line_number;

    g_input_buffer[g_input_buffer_length] = '\0';

    // Tokenize in-place.
    line_number = tokenize(g_input_buffer);
    if (line_number == 0xFFFF) {
        // Immediate mode.
        s = g_input_buffer;

        // Compile the line of BASIC.
        g_compiled_length = 0;

        do {
            int8_t error = 0;

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
            } else if (*s == T_LIST) {
                s += 1;
                add_call(list_statement);
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

        if (g_compiled_length > sizeof(g_compiled)) {
            // TODO: Check while adding bytes, not at the end.
            print("\n?Binary length exceeded");
        } else {
            // Call it.
            g_compiled_function();
        }
    } else {
        // Stored mode. Add line to program.

        // Return line to replace or delete, or location to insert new line.
        uint8_t *line = find_line(line_number);
        uint8_t *next_line = get_next_line(line);
        uint8_t *end_of_program = get_end_of_program(line);
        int16_t adjustment = 0;

        if (next_line == 0 || get_line_number(line) != line_number) {
            // Didn't find line. Insert it here.

            // Next pointer, line number, line, and nul.
            uint8_t buffer_length = strlen(g_input_buffer);
            adjustment = 4 + buffer_length + 1;

            // Shift rest of program over.
            memmove(line + adjustment, line, end_of_program - line);

            // Next line. Point to yourself initially, we'll adjust below.
            *((uint8_t **) line) = line;

            // Line number.
            *((uint16_t *) (line + 2)) = line_number;

            // Buffer and nul.
            memmove(line + 4, g_input_buffer, buffer_length + 1);
        } else {
            // Found line.

            if (g_input_buffer[0] == '\0') {
                // Empty line, delete old one.

                // Adjustment is negative.
                adjustment = line - next_line;
                memmove(line, next_line, end_of_program - next_line);
            } else {
                // Replace line.

                // Compute adjustment.
                uint8_t buffer_length = strlen(g_input_buffer);
                adjustment = line - next_line + 4 + buffer_length + 1;
                memmove(next_line + adjustment, next_line, end_of_program - next_line);

                // Buffer and nul.
                memmove(line + 4, g_input_buffer, buffer_length + 1);
            }
        }

        if (adjustment != 0) {
            // Adjust all the next pointers.
            while ((next_line = get_next_line(line)) != 0) {
                // Adjust by the amount we inserted or deleted.
                next_line += adjustment;

                *((uint8_t **) line) = next_line;
                line = next_line;
            }
        }
    }
}

int16_t main(void)
{
    int16_t blink;

    // Blank program.
    g_program[0] = '\0';
    g_program[1] = '\0';

    // Initialize UI.
    home();

    // Display the character set.
    if (0) {
        int16_t i;
        for (i = 0; i < 256; i++) {
            volatile uint8_t *loc;
            // Fails with: unhandled instruction B2
            move_cursor(i % 16, i >> 4);
            // Works.
            // move_cursor(i & 0x0F, i >> 4);
            loc = cursor_pos();
            *loc = i;
        }
        while(1);
    }

    // Print title.
    move_cursor((40 - title_length) / 2, 0);
    print(title);

    // Prompt.
    print("\n\n]");

    // Keyboard input.
    blink = 0;
    g_input_buffer_length = 0;
    show_cursor();
    while(1) {
        // Blink cursor.
        blink += 1;
        if (blink == 3000) {
            if (g_showing_cursor) {
                hide_cursor();
            } else {
                show_cursor();
            }
            blink = 0;
        }

        if(keyboard_test()) {
            hide_cursor();

            while(keyboard_test()) {
                uint8_t key;

                key = keyboard_get();
                if (key == 8) {
                    // Backspace.
                    if (g_input_buffer_length > 0) {
                        move_cursor(g_cursor_x - 1, g_cursor_y);
                        g_input_buffer_length -= 1;
                    }
                } else if (key == 13) {
                    // Return.
                    move_cursor(0, g_cursor_y + 1);

                    process_input_buffer();

                    print("\n]");
                    g_input_buffer_length = 0;
                } else {
                    if (g_input_buffer_length < sizeof(g_input_buffer) - 1) {
                        volatile uint8_t *loc = cursor_pos();
                        *loc = key | 0x80;
                        move_cursor(g_cursor_x + 1, g_cursor_y);

                        g_input_buffer[g_input_buffer_length++] = key;
                    }
                }
            }

            show_cursor();
        }
    }

    return 0;
}
