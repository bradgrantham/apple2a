#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include "platform.h"

// Line number used for "no line number".
#define INVALID_LINE_NUMBER 0xFFFF

// Maximum number of variables. These fit in the zero page.
#define MAX_VARIABLES 16

// Location of first variable in zero page. See zeropage.s and zeropage.inc
// in the compiler tree. They seem to use 26 bytes, so we start after that.
// Each variable takes two bytes (int16_t).
#define FIRST_VARIABLE 26

// Data types for variables.
#define DT_INT 0
#define DT_ARRAY 1

typedef struct {
    // The name of the variable, with the first letter in the lower byte
    // and the second letter (or nul) in the higher byte. Zero indicates
    // that this entry is unused.
    uint16_t name;

    // Data type of the variable. See DT_ constants above. This is also the
    // namespace that the variable name is in.
    uint8_t data_type;
} VarInfo;

extern uint16_t g_cursor_x;
extern uint16_t g_cursor_y;
extern uint16_t g_showing_cursor;
extern uint8_t g_cursor_ch;
extern VarInfo g_variables[MAX_VARIABLES];

void initialize_runtime(void);
void clear_for_stack(void);

uint8_t *cursor_pos(void);
void show_cursor(void);
void hide_cursor(void);
void move_cursor(int16_t x, int16_t y);

void clear_to_eol(void);

void home(void);

void allocate_array(uint16_t size, uint16_t var_addr);

void print(uint8_t *s);
void print_char(uint8_t c);
void print_uint(uint16_t i);
void print_int(int16_t i);
void print_newline(void);

void for_statement(uint16_t line_number, uint16_t var_address, int16_t end_value, int16_t step,
        uint16_t loop_top_addr);
uint16_t next_statement(uint16_t line_number, uint16_t var_address);

void syntax_error(uint16_t line_number);
void syntax_error_in_line(uint16_t line_number);
void undefined_statement_error(uint16_t line_number);
void redimd_array_error(uint16_t line_number);

void gr_statement(void);
void text_statement(void);
void color_statement(uint16_t color);
void plot_statement(uint16_t x, uint16_t y);

#endif // __RUNTIME_H__
