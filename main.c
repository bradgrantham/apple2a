#include <string.h>

#include "exporter.h"
#include "platform.h"
#include "runtime.h"

uint8_t *title = "Apple IIa";
uint8_t title_length = 9;

// 6502 instructions.
#define I_CLC 0x18
#define I_JSR 0x20
#define I_SEC 0x38
#define I_JMP_ABS 0x4C
#define I_RTS 0x60
#define I_STA_ZPG 0x85
#define I_STX_ZPG 0x86
#define I_STA_IND_Y 0x91
#define I_LDY_IMM 0xA0
#define I_LDX_IMM 0xA2
#define I_LDA_ZPG 0xA5
#define I_LDX_ZPG 0xA6
#define I_LDA_IMM 0xA9

// Tokens.
#define T_HOME 0x80
#define T_PRINT 0x81
#define T_LIST 0x82
#define T_POKE 0x83
#define T_RUN 0x84
#define T_NEW 0x85
#define T_PLUS 0x86
#define T_MINUS 0x87
#define T_TIMES 0x88
#define T_DIVIDE 0x89
#define T_CARET 0x8A
#define T_AND 0x8B
#define T_OR 0x8C
#define T_GREATER_THAN 0x8D
#define T_EQUALS 0x8E
#define T_LESS_THAN 0x8F
#define T_GOTO 0x90

// Line number used for "no line number".
#define INVALID_LINE_NUMBER 0xFFFF

// Variable for "No more space for variables".
#define OUT_OF_VARIABLE_SPACE 0xFF

// Maximum number of lines in stored program.
#define MAX_LINES 128

// Test for whether a character is a digit.
#define IS_DIGIT(ch) ((ch) >= '0' && (ch) <= '9')

// Test for first and subsequent variable name letters.
#define IS_FIRST_VARIABLE_LETTER(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define IS_SUBSEQUENT_VARIABLE_LETTER(ch) (IS_FIRST_VARIABLE_LETTER(ch) || IS_DIGIT(ch))

// List of tokens. The token value is the index plus 0x80.
static uint8_t *TOKEN[] = {
    "HOME",
    "PRINT",
    "LIST",
    "POKE",
    "RUN",
    "NEW",
    "+",
    "-",
    "*",
    "/",
    "^",
    "AND",
    "OR",
    ">",
    "=",
    "<",
    "GOTO",
};
static int16_t TOKEN_COUNT = sizeof(TOKEN)/sizeof(TOKEN[0]);

uint8_t g_input_buffer[40];
int16_t g_input_buffer_length = 0;

// Compiled binary.
uint8_t g_compiled[1024];
int16_t g_compiled_length = 0;
void (*g_compiled_function)() = (void (*)()) g_compiled;

// Stored program. Each line is:
// - Two bytes for pointer to next line (or zero if none).
// - Two bytes for line number.
// - Program line.
// - Nul.
uint8_t g_program[1024];

// Address of each line of code when compiled (for GOTO statements).
// Each line takes two words: one for the line number and
// one for the address in memory of the compiled code.
uint16_t g_line_address[MAX_LINES*2];
uint16_t g_line_address_count;

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

/**
 * Clear the stored program.
 */
static void new_statement() {
    g_program[0] = '\0';
    g_program[1] = '\0';
}

/**
 * List the stored program.
 */
static void list_statement() {
    uint8_t *line = g_program;
    uint8_t *next_line;

    print_newline();

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
 * Add a function call to the compiled buffer.
 */
static void add_call(void *function) {
    uint16_t addr = (uint16_t) function;

    g_compiled[g_compiled_length++] = I_JSR;
    g_compiled[g_compiled_length++] = addr & 0xFF;
    g_compiled[g_compiled_length++] = addr >> 8;
}

/**
 * Add a function return to the compiled buffer.
 */
static void add_return() {
    g_compiled[g_compiled_length++] = I_RTS;
}

/**
 * Parse an unsigned integer, returning the value and moving the pointer
 * past the end of the number. The pointer must already be at the beginning
 * of the number.
 */
static uint16_t parse_uint16(uint8_t **s_ptr) {
    uint16_t value = 0;
    uint8_t *s = *s_ptr;

    while (IS_DIGIT(*s)) {
        value = value*10 + (*s - '0');
        s += 1;
    }

    *s_ptr = s;

    return value;
}

/**
 * Generate code to put the value into AX.
 */
static void compile_load_ax(uint16_t value) {
    g_compiled[g_compiled_length++] = I_LDX_IMM;
    g_compiled[g_compiled_length++] = value >> 8;
    g_compiled[g_compiled_length++] = I_LDA_IMM;
    g_compiled[g_compiled_length++] = value & 0xFF;
}

/**
 * Find a variable by name. Only the first two letters are considered.
 * Advances the pointer past the variable name (including letters after
 * the first two). Returns the memory address of the variable. If we
 * ran out of space for variables, returns OUT_OF_VARIABLE_SPACE
 * and does not modify the buffer pointer.
 */
static uint8_t find_variable(uint8_t **buffer) {
    uint8_t *s = *buffer;
    uint8_t *existing_name = g_variable_names;
    uint8_t name[2];
    int16_t var;

    // Pull out the variable name.
    name[0] = *s++;
    if (IS_SUBSEQUENT_VARIABLE_LETTER(*s)) {
        name[1] = *s++;
    } else {
        name[1] = 0;
    }
    // Skip rest of name.
    while (IS_SUBSEQUENT_VARIABLE_LETTER(*s)) {
        s++;
    }

    for (var = 0; var < MAX_VARIABLES; var++) {
        if (existing_name[0] == 0 && existing_name[1] == 0) {
            // First free entry. Allocate it.
            existing_name[0] = name[0];
            existing_name[1] = name[1];
            break;
        } else if (existing_name[0] == name[0] && existing_name[1] == name[1]) {
            // Found it.
            break;
        }
        existing_name += 2;
    }

    if (var == MAX_VARIABLES) {
        var = OUT_OF_VARIABLE_SPACE;
    } else {
        // Convert index to address.
        var = FIRST_VARIABLE + 2*var;

        // Advance pointer.
        *buffer = s;
    }

    return (uint8_t) var;
}

/**
 * Find the address of a line in the compiled buffer, or 0xFFFF if not found.
 */
static uint16_t find_line_address(uint16_t line_number) {
    int i;

    for (i = 0; i < g_line_address_count; i++) {
        if (g_line_address[i*2] == line_number) {
            return g_line_address[i*2 + 1];
        }
    }

    return 0xFFFF;
}

/**
 * Parse an expression, generating code to compute it, leaving the
 * result in AX.
 */
static uint8_t *compile_expression(uint8_t *s) {
    int plus_count = 0;
    char have_value_in_ax = 0;

    while (1) {
        if (IS_DIGIT(*s)) {
            // Parse number.
            uint16_t value;

            if (have_value_in_ax) {
                // Push on the number stack.
                add_call(pushax);
            }

            value = parse_uint16(&s);
            compile_load_ax(value);
            have_value_in_ax = 1;
        } else if (IS_FIRST_VARIABLE_LETTER(*s)) {
            // Variable reference.
            uint8_t var = find_variable(&s);

            if (have_value_in_ax) {
                // Push on the number stack.
                add_call(pushax);
            }

            if (var == OUT_OF_VARIABLE_SPACE) {
                // TODO: Not sure how to deal with this. For now just
                // fill in with zero, since assigning to this elsewhere
                // will cause an error.
                compile_load_ax(0);
            } else {
                // Load from var.
                g_compiled[g_compiled_length++] = I_LDA_ZPG;
                g_compiled[g_compiled_length++] = var;
                g_compiled[g_compiled_length++] = I_LDX_ZPG;
                g_compiled[g_compiled_length++] = var + 1;
            }
            have_value_in_ax = 1;
        } else if (*s == T_PLUS) {
            plus_count += 1;
            s += 1;
        } else {
            break;
        }
    }

    while (plus_count > 0) {
        add_call(tosaddax);
        plus_count -= 1;
    }

    return s;
}

/**
 * Tokenize a string in place. Returns (and removes) any line number, or
 * INVALID_LINE_NUMBER if there's none.
 */
static uint16_t tokenize(uint8_t *s) {
    uint8_t *t = s; // Tokenized version.
    int16_t line_number;

    // Parse optional line number.
    if (IS_DIGIT(*s)) {
        line_number = parse_uint16(&s);
    } else {
        line_number = INVALID_LINE_NUMBER;
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
 * Call to configure the compilation step.
 */
static void set_up_compile(void) {
    g_compiled_length = 0;
    g_line_address_count = 0;
}

/**
 * Compile the tokenized line of BASIC, adding it to the g_compiled binary.
 */
static void compile_buffer(uint8_t *buffer, uint16_t line_number) {
    uint8_t *s = buffer;
    uint8_t done;

    do {
        int8_t error = 0;

        // Default to being done after one statement.
        done = 1;

        if (*s == '\0' || *s == ':') {
            // Empty statement. We skip the colon below.
        } else if (IS_FIRST_VARIABLE_LETTER(*s)) {
            // Must be variable assignment.
            uint8_t var = find_variable(&s);
            if (var == OUT_OF_VARIABLE_SPACE) {
                // TODO: Nicer error specifically for out of variable space.
                error = 1;
            } else {
                if (*s != T_EQUALS) {
                    error = 1;
                } else {
                    s += 1;
                    // Parse address.
                    s = compile_expression(s);
                    // Copy to var.
                    g_compiled[g_compiled_length++] = I_STA_ZPG;
                    g_compiled[g_compiled_length++] = var;
                    g_compiled[g_compiled_length++] = I_STX_ZPG;
                    g_compiled[g_compiled_length++] = var + 1;
                }
            }
        } else if (*s == T_HOME) {
            s += 1;
            add_call(home);
        } else if (*s == T_PRINT) {
            s += 1;

            if (*s != '\0' && *s != ':') {
                // Parse expression.
                s = compile_expression(s);
                add_call(print_int);
            }

            add_call(print_newline);
        } else if (*s == T_LIST) {
            s += 1;
            add_call(list_statement);
        } else if (*s == T_POKE) {
            s += 1;
            // Parse address.
            s = compile_expression(s);
            // Copy from AX to ptr1.
            g_compiled[g_compiled_length++] = I_STA_ZPG;
            g_compiled[g_compiled_length++] = (uint8_t) &ptr1;
            g_compiled[g_compiled_length++] = I_STX_ZPG;
            g_compiled[g_compiled_length++] = (uint8_t) &ptr1 + 1;
            if (*s != ',') {
                error = 1;
            } else {
                s++;
                // Parse value. LSB is in A.
                s = compile_expression(s);
                g_compiled[g_compiled_length++] = I_LDY_IMM;
                g_compiled[g_compiled_length++] = 0;
                g_compiled[g_compiled_length++] = I_STA_IND_Y;
                g_compiled[g_compiled_length++] = (uint8_t) &ptr1;
            }
        } else if (*s == T_GOTO) {
            s += 1;

            if (!IS_DIGIT(*s)) {
                error = 1;
            } else {
                uint16_t target_line_number = parse_uint16(&s);
                uint16_t addr = find_line_address(target_line_number);

                if (addr == 0xFFFF) {
                    // Line not found.
                    // TODO better error message.
                    error = 1;
                } else {
                    g_compiled[g_compiled_length++] = I_JMP_ABS;
                    g_compiled[g_compiled_length++] = addr & 0xFF;
                    g_compiled[g_compiled_length++] = addr >> 8;
                }
            }
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
            if (line_number != INVALID_LINE_NUMBER) {
                compile_load_ax(line_number);
                add_call(syntax_error_in_line);
            } else {
                add_call(syntax_error);
            }
        }
    } while (!done);
}

/**
 * Complete the compilation buffer and run it.
 */
static void complete_compile_and_execute(void) {
    // Return from function.
    add_return();

    // Dump compiled buffer to the terminal.
    {
        int i;
        uint8_t *debug_port = (uint8_t *) 0xBFFE;

        // Size of program (including initial address).
        debug_port[0] = 2 + g_compiled_length;
        // Address of program start, little endian.
        debug_port[1] = ((uint16_t) &g_compiled[0]) & 0xFF;
        debug_port[1] = ((uint16_t) &g_compiled[0]) >> 8;
        // Program bytes.
        for (i = 0; i < g_compiled_length; i++) {
            debug_port[1] = g_compiled[i];
        }
    }

    if (g_compiled_length > sizeof(g_compiled)) {
        // TODO: Check while adding bytes, not at the end.
        print("\n?Binary length exceeded");
    } else {
        // Call it.
        g_compiled_function();
    }
}

/**
 * Clear out all variables. This does not clear their value, only our
 * knowledge of them.
 */
void clear_variables(void) {
    memset(g_variable_names, 0, sizeof(g_variable_names));
}

/**
 * Compile the stored program and execute it.
 */
static void compile_stored_program(void) {
    uint8_t *line = g_program;
    uint8_t *next_line;

    // Clear out all variables.
    clear_variables();

    set_up_compile();

    // Generate code to zero out all variable values. Do this in the program
    // itself because each RUN should clear them out.
    add_call(clear_variable_values);

    while ((next_line = get_next_line(line)) != 0) {
        uint16_t line_number = get_line_number(line);

        // Store address of line in compiled buffer.
        if (g_line_address_count == MAX_LINES) {
            // TODO not sure what to do here.
            print("Program too large");
            break;
        } else {
            g_line_address[g_line_address_count++] = line_number;
            g_line_address[g_line_address_count++] = (uint16_t) (g_compiled + g_compiled_length);
        }

        compile_buffer(line + 4, line_number);

        line = next_line;
    }
    complete_compile_and_execute();
}

/**
 * Process the user's line of input, possibly compiling the code.
 * and executing it.
 */
static void process_input_buffer() {
    uint16_t line_number;

    g_input_buffer[g_input_buffer_length] = '\0';

    // Tokenize in-place.
    line_number = tokenize(g_input_buffer);
    if (line_number == INVALID_LINE_NUMBER) {
        // Immediate mode.

        if (g_input_buffer[0] == T_RUN) {
            // We don't compile "RUN".
            compile_stored_program();
        } else if (g_input_buffer[0] == T_NEW) {
            // We don't compile "NEW".
            new_statement();
        } else {
            // Compile the immediate mode line.
            set_up_compile();
            compile_buffer(g_input_buffer, INVALID_LINE_NUMBER);
            complete_compile_and_execute();
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

    // Clear stored program.
    new_statement();

    // Clear out all variables.
    clear_variables();

    // Initialize UI.
    home();

    // Display the character set.
    /*
    if (1) {
        int16_t i;
        for (i = 0; i < 256; i++) {
            uint8_t *loc;
            // Fails with: unhandled instruction B2
            move_cursor(i % 16, i >> 4);
            // Works.
            // move_cursor(i & 0x0F, i >> 4);
            loc = cursor_pos();
            *loc = i;
        }
        while(1);
    }
    */

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
                    clear_to_eol();
                    print_char('\n');

                    process_input_buffer();

                    print("\n]");
                    g_input_buffer_length = 0;
                } else {
                    if (g_input_buffer_length < sizeof(g_input_buffer) - 1) {
                        uint8_t *loc = cursor_pos();
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
