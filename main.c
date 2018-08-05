#include <string.h>

#include "exporter.h"
#include "platform.h"
#include "runtime.h"

uint8_t *title = "Apple IIa";
uint8_t title_length = 9;

// 6502 instructions.
#define I_ORA_ZPG 0x05
#define I_CLC 0x18
#define I_JSR 0x20
#define I_SEC 0x38
#define I_JMP_ABS 0x4C
#define I_RTS 0x60
#define I_JMP_IND 0x6C
#define I_STA_ZPG 0x85
#define I_STX_ZPG 0x86
#define I_STA_IND_Y 0x91
#define I_LDY_IMM 0xA0
#define I_LDX_IMM 0xA2
#define I_LDA_ZPG 0xA5
#define I_LDX_ZPG 0xA6
#define I_LDA_IMM 0xA9
#define I_TAX 0xAA
#define I_BNE_REL 0xD0
#define I_BEQ_REL 0xF0

// Tokens.
#define T_HOME 0x80
#define T_PRINT 0x81
#define T_LIST 0x82
#define T_POKE 0x83
#define T_RUN 0x84
#define T_NEW 0x85
#define T_PLUS 0x86
#define T_MINUS 0x87
#define T_ASTERISK 0x88
#define T_SLASH 0x89
#define T_CARET 0x8A
#define T_AND 0x8B
#define T_OR 0x8C
#define T_GREATER_THAN 0x8D
#define T_EQUAL 0x8E
#define T_LESS_THAN 0x8F
#define T_GOTO 0x90
#define T_IF 0x91
#define T_THEN 0x92
#define T_GR 0x93
#define T_TEXT 0x94
#define T_COLOR 0x95
#define T_PLOT 0x96
#define T_FOR 0x97
#define T_TO 0x98
#define T_STEP 0x99
#define T_NEXT 0x9A
#define T_NOT 0x9B

// Operators. These encode both the operator (high nybble) and the precedence
// (low nybble). Lower precedence has a lower low nybble value. For example,
// OP_ADD (0x99) and OP_SUB (0xA9) have the same precedence (9). By convention
// the precedence is the value of the lowest-valued operator in its class
// (OP_ADD = 0x99), but only the relative values of precedence matter. All
// of these are left-associative, as in AppleSoft BASIC. (Even though
// exponentiation really should be right-associative.)
#define OP_PRECEDENCE(op) ((op) & 0x0F)
#define OP_OR 0x00
#define OP_AND 0x11
#define OP_NOT 0x22
#define OP_LTE 0x33
#define OP_GTE 0x43
#define OP_EQ 0x55
#define OP_NEQ 0x65
#define OP_LT 0x75
#define OP_GT 0x85
#define OP_ADD 0x99
#define OP_SUB 0xA9
#define OP_MULT 0xBB
#define OP_DIV 0xCB
#define OP_NEG 0xDD
#define OP_EXP 0xEE
#define OP_CLOSE_PARENS 0xFD // Never on the stack.
#define OP_OPEN_PARENS 0xFE // Ignore precedence.
#define OP_INVALID 0xFF

// Variable for "No more space for variables".
#define OUT_OF_VARIABLE_SPACE 0xFF

// Maximum number of lines in stored program.
#define MAX_LINES 128

// Maximum number of operators in the operator stack.
#define MAX_OP_STACK 16

// Maximum number of forward GOTOs.
#define MAX_FORWARD_GOTO 16

// Test for whether a character is a digit.
#define IS_DIGIT(ch) ((ch) >= '0' && (ch) <= '9')

// Test for first and subsequent variable name letters.
#define IS_FIRST_VARIABLE_LETTER(ch) ((ch) >= 'A' && (ch) <= 'Z')
#define IS_SUBSEQUENT_VARIABLE_LETTER(ch) (IS_FIRST_VARIABLE_LETTER(ch) || IS_DIGIT(ch))

// Info for each "forward GOTO", which is a GOTO to a line that we've
// not compiled yet.
typedef struct {
    // The line number the GOTO is on. This is for error messages.
    uint16_t source_line_number;

    // The line number it's trying to jump to.
    uint16_t target_line_number;

    // The address of the JMP instructions. This is 0 if this entry is unused.
    uint8_t *jmp_address;
} ForwardGoto;

// Info for each compiled line.
typedef struct {
    // The line's number.
    uint16_t line_number;

    // The address in memory where its code was compiled.
    uint8_t *code;
} LineInfo;

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
    "IF",
    "THEN",
    "GR",
    "TEXT",
    "COLOR",
    "PLOT",
    "FOR",
    "TO",
    "STEP",
    "NEXT",
    "NOT",
};
static int16_t TOKEN_COUNT = sizeof(TOKEN)/sizeof(TOKEN[0]);

uint8_t g_input_buffer[80];
int16_t g_input_buffer_length = 0;

// Compiled binary.
uint8_t g_compiled[1024];
uint8_t *g_c = g_compiled;
void (*g_compiled_function)() = (void (*)()) g_compiled;

// Stored program. Each line is:
// - Two bytes for pointer to next line (or zero if none).
// - Two bytes for line number.
// - Program line.
// - Nul.
uint8_t g_program[1024];

// Info about each compiled line.
LineInfo g_line_info[MAX_LINES];
uint8_t g_line_info_count;

// Operator stack, of the expression-evaluation routines. These are from the
// OP_ constants.
uint8_t g_op_stack[MAX_OP_STACK];
uint8_t g_op_stack_size = 0;

// List of all forward GOTOs. These are packed at the beginning, so the
// first invalid (jmp_address == 0) entry marks the end.
ForwardGoto g_forward_goto[MAX_FORWARD_GOTO];
uint8_t g_forward_goto_count = 0;

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

    g_c[0] = I_JSR;
    g_c[1] = addr & 0xFF;
    g_c[2] = addr >> 8;
    g_c += 3;
}

/**
 * Add a function return to the compiled buffer.
 */
static void add_return() {
    *g_c++ = I_RTS;
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
    g_c[0] = I_LDX_IMM;
    g_c[1] = value >> 8;
    g_c[2] = I_LDA_IMM;
    g_c[3] = value & 0xFF;
    g_c += 4;
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
 * Find the address of a line in the compiled buffer, or 0 if not found.
 */
static uint8_t *find_line_address(uint16_t line_number) {
    int i;

    for (i = 0; i < g_line_info_count; i++) {
        LineInfo *l = &g_line_info[i];

        if (l->line_number == line_number) {
            return l->code;
        }
    }

    return 0;
}

/**
 * Pop an operator off the operator stack and compile it.
 */
static void pop_operator_stack() {
    uint8_t op = g_op_stack[--g_op_stack_size];

    switch (op) {
        case OP_ADD:
            add_call(tosaddax);
            break;

        case OP_SUB:
            add_call(tossubax);
            break;

        case OP_MULT:
            add_call(tosmulax);
            break;

        case OP_DIV:
            add_call(tosdivax);
            break;

        case OP_EQ:
            add_call(toseqax);
            break;

        case OP_NEQ:
            add_call(tosneax);
            break;

        case OP_LT:
            add_call(tosltax);
            break;

        case OP_GT:
            add_call(tosgtax);
            break;

        case OP_LTE:
            add_call(tosleax);
            break;

        case OP_GTE:
            add_call(tosgeax);
            break;

        case OP_AND:
            // AppleSoft BASIC does not have short-circuit logical operators.

            // See if second operand is 0.
            g_c[0] = I_STX_ZPG;
            g_c[1] = (uint8_t) &tmp1;
            g_c[2] = I_ORA_ZPG;
            g_c[3] = (uint8_t) &tmp1;
            // If so, skip other test. A contains 0.
            g_c[4] = I_BEQ_REL;
            g_c[5] = 11; // 3 + 6 + 2
            g_c += 6;
            add_call(popax); // 3 instructions
            // See if first operand is 0.
            g_c[0] = I_STX_ZPG;
            g_c[1] = (uint8_t) &tmp1;
            g_c[2] = I_ORA_ZPG;
            g_c[3] = (uint8_t) &tmp1;
            // If so, skip setting A to 1. A contains 0.
            g_c[4] = I_BEQ_REL;
            g_c[5] = 2; // The LDA below.
            g_c += 6;
            // Set A to 1.
            g_c[0] = I_LDA_IMM;
            g_c[1] = 1;
            g_c[2] = I_LDX_IMM;     // The BEQs above arrive here.
            g_c[3] = 0;
            g_c += 4;
            break;

        case OP_OR:
            // AppleSoft BASIC does not have short-circuit logical operators.

            // See if second operand is 0.
            g_c[0] = I_STX_ZPG;
            g_c[1] = (uint8_t) &tmp1;
            g_c[2] = I_ORA_ZPG;
            g_c[3] = (uint8_t) &tmp1;
            // If not, skip other test.
            g_c[4] = I_BNE_REL;
            g_c[5] = 9; // 3 + 6
            g_c += 6;
            add_call(popax); // 3 instructions
            // See if first operand is 0.
            g_c[0] = I_STX_ZPG;
            g_c[1] = (uint8_t) &tmp1;
            g_c[2] = I_ORA_ZPG;
            g_c[3] = (uint8_t) &tmp1;
            // If so, skip setting A to 1. A contains 0.
            g_c[4] = I_BEQ_REL;
            g_c[5] = 2; // The LDA below.
            g_c += 6;
            // Set A to 1.
            g_c[0] = I_LDA_IMM;     // The BNE arrives here.
            g_c[1] = 1;
            g_c[2] = I_LDX_IMM;     // The BEQ arrives here.
            g_c[3] = 0;
            g_c += 4;
            break;

        case OP_NOT:
            add_call(bnegax);
            break;

        case OP_NEG:
            add_call(negax);
            break;

        case OP_OPEN_PARENS:
            // No-op.
            break;

        default:
            print("Unhandled operator\n");
            break;
    }
}

/**
 * Push an operator onto the operator stack. Follow the Shunting-yard
 * algorithm so that higher-precedence operators are performed
 * first.
 *
 * https://en.wikipedia.org/wiki/Shunting-yard_algorithm
 */
static void push_operator_stack(uint8_t op) {
    // Don't pop anything off if op is unary.
    if (op != OP_NOT && op != OP_NEG) {
        // All our operators are left-associative, so no special check for the case
        // of equal precedence.
        while (g_op_stack_size > 0 &&
                g_op_stack[g_op_stack_size - 1] != OP_OPEN_PARENS &&
                OP_PRECEDENCE(g_op_stack[g_op_stack_size - 1]) >= OP_PRECEDENCE(op)) {

            pop_operator_stack();
        }
    }

    // TODO Check for g_op_stack overflow.
    g_op_stack[g_op_stack_size++] = op;
}

/**
 * Parse an expression, generating code to compute it, leaving the
 * result in AX.
 */
static uint8_t *compile_expression(uint8_t *s) {
    char have_value_in_ax = 0;
    uint8_t expect_unary_minus = 1; // Expect unary minus at start of expression.

    while (1) {
        if (IS_DIGIT(*s)) {
            // Parse number.
            if (have_value_in_ax) {
                // Push on the number stack.
                add_call(pushax);
            }

            compile_load_ax(parse_uint16(&s));
            have_value_in_ax = 1;

            // Expect binary minus after operand.
            expect_unary_minus = 0;
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
                g_c[0] = I_LDA_ZPG;
                g_c[1] = var;
                g_c[2] = I_LDX_ZPG;
                g_c[3] = var + 1;
                g_c += 4;
            }
            have_value_in_ax = 1;

            // Expect binary minus after operand.
            expect_unary_minus = 0;
        } else {
            // Check if it's an operator.
            uint8_t op = OP_INVALID;

            if (*s == T_PLUS) {
                op = OP_ADD;
            } else if (*s == T_MINUS) {
                op = expect_unary_minus ? OP_NEG : OP_SUB;
            } else if (*s == T_ASTERISK) {
                op = OP_MULT;
            } else if (*s == T_SLASH) {
                op = OP_DIV;
            } else if (*s == T_AND) {
                op = OP_AND;
            } else if (*s == T_OR) {
                op = OP_OR;
            } else if (*s == T_NOT) {
                op = OP_NOT;
            } else if (*s == T_EQUAL) {
                if (s[1] == T_LESS_THAN) {
                    s += 1;
                    op = OP_LTE;
                } else if (s[1] == T_GREATER_THAN) {
                    s += 1;
                    op = OP_GTE;
                } else {
                    op = OP_EQ;
                }
            } else if (*s == T_LESS_THAN) {
                if (s[1] == T_EQUAL) {
                    s += 1;
                    op = OP_LTE;
                } else if (s[1] == T_GREATER_THAN) {
                    s += 1;
                    op = OP_NEQ;
                } else {
                    op = OP_LT;
                }
            } else if (*s == T_GREATER_THAN) {
                if (s[1] == T_EQUAL) {
                    s += 1;
                    op = OP_GTE;
                } else if (s[1] == T_LESS_THAN) {
                    s += 1;
                    op = OP_NEQ;
                } else {
                    op = OP_GT;
                }
            } else if (*s == '(') { // Parentheses are not tokenized.
                op = OP_OPEN_PARENS;
            } else if (*s == ')') { // Parentheses are not tokenized.
                op = OP_CLOSE_PARENS;

                // Pop until open parethesis.
                while (g_op_stack_size > 0 && g_op_stack[g_op_stack_size - 1] != OP_OPEN_PARENS) {
                    pop_operator_stack();
                }
                if (g_op_stack_size == 0) {
                    print("Extra close parenthesis\n");
                } else {
                    // Pop open parenthesis.
                    pop_operator_stack();
                }
            }

            // Expect unary minus after operators or open parenthesis. Expect
            // binary minus after close parenthesis.
            expect_unary_minus = op != OP_CLOSE_PARENS;

            if (op != OP_INVALID) {
                s += 1;
                if (op != OP_CLOSE_PARENS) {
                    push_operator_stack(op);
                }
            } else {
                break;
            }
        }
    }

    if (have_value_in_ax) {
        // Empty the operator stack.
        while (g_op_stack_size > 0) {
            if (g_op_stack[g_op_stack_size - 1] == OP_OPEN_PARENS) {
                print("Extra open parenthesis\n");
            }
            pop_operator_stack();
        }
    } else {
        // Something went wrong, we never got anything.
        print("Expression has no content\n");
        compile_load_ax(0);
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

            // Try every token.
            for (i = 0; i < TOKEN_COUNT; i++) {
                // Quick optimization, peek at the first letter.
                skipped = s[0] == TOKEN[i][0] ? skip_over(s, TOKEN[i]) : 0;
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
 * Adds a new entry to the list for forward GOTOs. Returns whether successful.
 */
static uint8_t add_forward_goto(uint16_t source_line_number, uint16_t target_line_number,
        uint8_t *jmp_address) {

    ForwardGoto *f;

    if (g_forward_goto_count == MAX_FORWARD_GOTO) {
        return 0;
    }

    f = &g_forward_goto[g_forward_goto_count++];
    f->source_line_number = source_line_number;
    f->target_line_number = target_line_number;
    f->jmp_address = jmp_address;

    return 1;
}

/**
 * Go through the list of forward GOTOs and set their jumps to this code.
 */
static void fix_up_forward_gotos(uint16_t line_number, uint8_t *code) {
    int i;
    ForwardGoto *f;
    uint16_t addr = (uint16_t) code;

    for (i = 0; i < g_forward_goto_count; i++) {
        f = &g_forward_goto[i];

        if (f->target_line_number == line_number) {
            // Fill in jump address.
            f->jmp_address[1] = addr & 0xFF;
            f->jmp_address[2] = addr >> 8;

            // Swap last entry with this one. It's okay if these
            // are the same entry.
            *f = g_forward_goto[g_forward_goto_count - 1];

            // Reduce size of array.
            g_forward_goto_count -= 1;

            // Re-process this entry, since we've swapped it.
            i -= 1;
        }
    }
}

/**
 * Adds an entry to the list of line infos. Returns whether successful.
 */
static uint8_t add_line_info(uint16_t line_number, uint8_t *code) {
    LineInfo *l;

    if (g_line_info_count == MAX_LINES) {
        // TODO not sure what to do here.
        print("Program too large");
        return 0;
    }

    l = &g_line_info[g_line_info_count++];
    l->line_number = line_number;
    l->code = code;

    // Fix up any forward GOTOs to this line.
    fix_up_forward_gotos(line_number, code);

    return 1;
}

/**
 * Call to configure the compilation step.
 */
static void set_up_compile(void) {
    g_c = g_compiled;
    g_line_info_count = 0;
    g_forward_goto_count = 0;
}

/**
 * Compile the tokenized line of BASIC, adding it to the g_compiled binary.
 */
static void compile_buffer(uint8_t *buffer, uint16_t line_number) {
    uint8_t *s = buffer;
    uint8_t done;
    // Keep track of addresses that point to the end of the line.
    uint8_t **end_of_line_address[4];
    uint8_t end_of_line_count = 0;

    do {
        int8_t error = 0;
        int8_t continue_statement = 0;

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
                if (*s != T_EQUAL) {
                    error = 1;
                } else {
                    s += 1;
                    // Parse value.
                    s = compile_expression(s);
                    // Copy to var.
                    g_c[0] = I_STA_ZPG;
                    g_c[1] = var;
                    g_c[2] = I_STX_ZPG;
                    g_c[3] = var + 1;
                    g_c += 4;
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
            g_c[0] = I_STA_ZPG;
            g_c[1] = (uint8_t) &ptr1;
            g_c[2] = I_STX_ZPG;
            g_c[3] = (uint8_t) &ptr1 + 1;
            g_c += 4;
            if (*s != ',') {
                error = 1;
            } else {
                s++;
                // Parse value. LSB is in A.
                s = compile_expression(s);
                g_c[0] = I_LDY_IMM;        // Zero out Y.
                g_c[1] = 0;
                g_c[2] = I_STA_IND_Y;      // Store at *ptr1.
                g_c[3] = (uint8_t) &ptr1;
                g_c += 4;
            }
        } else if (*s == T_GOTO) {
            s += 1;

            if (!IS_DIGIT(*s)) {
                error = 1;
            } else {
                uint16_t target_line_number = parse_uint16(&s);
                uint16_t addr = (uint16_t) find_line_address(target_line_number);

                if (addr == 0) {
                    // Line not found. Must be a forward GOTO. Record it
                    // and keep going.
                    uint8_t success = add_forward_goto(line_number, target_line_number,
                            g_c);
                    if (!success) {
                        // TODO handle error.
                    }
                }

                g_c[0] = I_JMP_ABS;
                g_c[1] = addr & 0xFF;
                g_c[2] = addr >> 8;
                g_c += 3;
            }
        } else if (*s == T_IF) {
            // Save where we are in case we need to roll back.
            uint8_t *saved_c = g_c;

            s += 1;
            // Parse conditional expression.
            s = compile_expression(s);
            // Check if AX is zero. Or the two bytes together, through the zero page.
            g_c[0] = I_STX_ZPG;
            g_c[1] = (uint8_t) &tmp1;
            g_c[2] = I_ORA_ZPG;
            g_c[3] = (uint8_t) &tmp1;
            // If so, skip to end of this line.
            g_c[4] = I_BNE_REL;
            g_c[5] = 3; // Skip over absolute jump.
            g_c[6] = I_JMP_ABS;
            g_c += 7;
            // TODO Check for overflow of end_of_line_address:
            end_of_line_address[end_of_line_count++] = (uint8_t **) g_c;
            g_c[0] = 0; // Address of next line.
            g_c[1] = 0; // Address of next line.
            g_c += 2;

            if (*s == T_THEN) {
                // Skip THEN and continue
                s += 1;
                continue_statement = 1;
            } else if (*s == T_GOTO) {
                // Just continue, we'll pick it up after the loop.
                continue_statement = 1;
            } else {
                // Must be THEN or GOTO. Erase what we've done.
                g_c = saved_c;
                error = 1;
            }
        } else if (*s == T_FOR) {
            uint8_t *loop_top_addr_addr = 0;

            s += 1;

            // We'll set this to 0 if we succeed.
            error = 1;

            if (IS_FIRST_VARIABLE_LETTER(*s)) {
                uint8_t var;

                // For the error message.
                compile_load_ax(line_number);
                add_call(pushax);

                var = find_variable(&s);
                if (var == OUT_OF_VARIABLE_SPACE) {
                    // TODO: Nicer error specifically for out of variable space.
                } else {
                    compile_load_ax(var);
                    add_call(pushax);

                    if (*s == T_EQUAL) {
                        s += 1;

                        // Parse initial value.
                        s = compile_expression(s);

                        // Copy to var.
                        g_c[0] = I_STA_ZPG;
                        g_c[1] = var;
                        g_c[2] = I_STX_ZPG;
                        g_c[3] = var + 1;
                        g_c += 4;

                        if (*s == T_TO) {
                            s += 1;

                            // Parse end value.
                            s = compile_expression(s);
                            add_call(pushax);

                            if (*s == T_STEP) {
                                s += 1;

                                // Parse step.
                                s = compile_expression(s);
                            } else {
                                // Default to step of 1.
                                compile_load_ax(1);
                            }
                            add_call(pushax);

                            // Finally, the address at the top of the FOR loop.
                            // We don't have it yet, so we leave space and record
                            // the location where the NEXT should jump to.
                            // Don't use compile_load_ax() here because a change
                            // there would mess up how we fill it in below. Inline
                            // it here so we have control over that.
                            loop_top_addr_addr = g_c;
                            g_c[0] = I_LDX_IMM;
                            g_c[2] = I_LDA_IMM;
                            g_c += 4;

                            add_call(for_statement);
                            error = 0;
                        }
                    }
                }
            }

            if (loop_top_addr_addr != 0) {
                uint16_t loop_top_addr = (uint16_t) g_c;
                loop_top_addr_addr[1] = loop_top_addr >> 8;     // X
                loop_top_addr_addr[3] = loop_top_addr & 0xFF;   // A
            }
        } else if (*s == T_NEXT) {
            s += 1;

            // For the error message.
            compile_load_ax(line_number);
            add_call(pushax);

            // See if there's the optional variable. We don't support multiple
            // variables ("NEXT I,J").
            if (IS_FIRST_VARIABLE_LETTER(*s)) {
                uint8_t var = find_variable(&s);
                if (var == OUT_OF_VARIABLE_SPACE) {
                    // TODO: Nicer error specifically for out of variable space.
                    error = 1;
                } else {
                    compile_load_ax(var);
                }
            } else {
                // Zero means find the most recent FOR loop.
                compile_load_ax(0);
            }

            // Process the NEXT instruction.
            add_call(next_statement);

            // The next_statement() function returns the address to jump
            // to if we're looping, or 0 if we're not.

            // Copy from AX to ptr1. We must save it because checking it destroys it.
            g_c[0] = I_STA_ZPG;
            g_c[1] = (uint8_t) &ptr1;
            g_c[2] = I_STX_ZPG;
            g_c[3] = (uint8_t) &ptr1 + 1;
            // Check if AX is zero. Destroys AX.
            g_c[4] = I_ORA_ZPG;
            g_c[5] = (uint8_t) &ptr1 + 1;  // OR X into A.
            // If zero, skip over jump.
            g_c[6] = I_BEQ_REL;
            g_c[7] = 3;                    // Skip over indirect jump.
            // Jump to top of loop, indirectly through ptr1, which has the address.
            g_c[8] = I_JMP_IND;
            g_c[9] = (uint8_t) &ptr1 & 0x0F;
            g_c[10] = (uint8_t) &ptr1 >> 8;
            g_c += 11;
        } else if (*s == T_GR) {
            s += 1;
            add_call(gr_statement);
        } else if (*s == T_TEXT) {
            s += 1;
            add_call(text_statement);
        } else if (*s == T_COLOR) {
            s += 1;
            if (*s != T_EQUAL) {
                error = 1;
            } else {
                s += 1;
                s = compile_expression(s);
                add_call(color_statement);
            }
        } else if (*s == T_PLOT) {
            s += 1;
            s = compile_expression(s);
            add_call(pushax);
            if (*s != ',') {
                error = 1;
            } else {
                s += 1;
                s = compile_expression(s);
                add_call(plot_statement);
            }
        } else {
            error = 1;
        }

        // Now we're at the end of our statement.
        if (!error) {
            if (continue_statement) {
                // No problem, just continue from here.
                done = 0;
            } else if (*s == ':') {
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
            end_of_line_count = 0;
            compile_load_ax(line_number);
            add_call(syntax_error);

            // Terminate program.
            add_return();
        }
    } while (!done);

    // Fill in the places where we needed the address of the end of the line.
    while (end_of_line_count > 0) {
        *end_of_line_address[--end_of_line_count] = g_c;
    }
}

/**
 * Complete the compilation buffer and run it.
 */
static void complete_compile_and_execute(void) {
    int i;
    uint16_t compiled_length;

    // Return from function.
    add_return();

    // Always clear the FOR stack before running. We don't want it
    // either in stored program mode, or in immediate mode.
    clear_for_stack();

    // Forward GOTOs that couldn't be resolved are changed to
    // jumps to error messages.
    for (i = 0; i < g_forward_goto_count; i++) {
        ForwardGoto *f = &g_forward_goto[i];
        uint16_t addr = (uint16_t) g_c;

        // Jump to end of buffer.
        f->jmp_address[1] = addr & 0xFF;
        f->jmp_address[2] = addr >> 8;

        // Add code at end of buffer to show error.
        compile_load_ax(f->source_line_number);
        add_call(undefined_statement_error);

        // Terminate program.
        add_return();
    }

    // Dump compiled buffer to the terminal.
    compiled_length = g_c - g_compiled;
    if (1) {
        int i;
        uint8_t *debug_port = (uint8_t *) 0xBFFE;

        // Size of program (including initial address).
        debug_port[0] = 2 + compiled_length;
        // Address of program start, little endian.
        debug_port[1] = ((uint16_t) &g_compiled[0]) & 0xFF;
        debug_port[1] = ((uint16_t) &g_compiled[0]) >> 8;
        // Program bytes.
        for (i = 0; i < compiled_length; i++) {
            debug_port[1] = g_compiled[i];
        }
    }

    if (compiled_length > sizeof(g_compiled)) {
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

    // Clear runtime state.
    add_call(initialize_runtime);

    while ((next_line = get_next_line(line)) != 0) {
        uint16_t line_number = get_line_number(line);
        uint8_t success = add_line_info(line_number, g_c);

        // Compile just this line.
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

    /*
       // For testing generated code. TODO remove
    {
        int16_t a, b, c;
        b = 5;
        c = 6;
        a = b && c;
    }
    */

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
                        print_char(key);
                        g_input_buffer[g_input_buffer_length++] = key;
                    }
                }
            }

            show_cursor();
        }
    }

    return 0;
}
