#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include "platform.h"

// Maximum number of variables. These fit in the zero page.
#define MAX_VARIABLES 16

// Location of first variable in zero page. See zeropage.s and zeropage.inc
// in the compiler tree. They seem to use 26 bytes, so we start after that.
// Each variable takes two bytes (int16_t).
#define FIRST_VARIABLE 26

extern uint16_t g_cursor_x;
extern uint16_t g_cursor_y;
extern uint16_t g_showing_cursor;
extern uint8_t g_cursor_ch;
extern uint8_t g_variable_names[MAX_VARIABLES*2];

void clear_variable_values(void);

uint8_t *cursor_pos(void);
void show_cursor(void);
void hide_cursor(void);
void move_cursor(int16_t x, int16_t y);

void clear_to_eol(void);

void home(void);

void print(uint8_t *s);
void print_char(uint8_t c);
void print_int(uint16_t i);
void print_newline(void);

void syntax_error(void);
void syntax_error_in_line(uint16_t line_number);

void gr_statement(void);
void text_statement(void);

#endif // __RUNTIME_H__
