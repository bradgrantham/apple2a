#ifndef __RUNTIME_H__
#define __RUNTIME_H__

#include "platform.h"

extern uint16_t g_cursor_x;
extern uint16_t g_cursor_y;
extern uint16_t g_showing_cursor;
extern uint8_t g_cursor_ch;

uint8_t *cursor_pos(void);
void show_cursor(void);
void hide_cursor(void);
void move_cursor(int16_t x, int16_t y);

void home(void);

void print(uint8_t *s);
void print_char(uint8_t c);
void print_int(uint16_t i);
void print_newline(void);

void syntax_error(void);

#endif // __RUNTIME_H__
