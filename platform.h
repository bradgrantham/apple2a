#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#define TEXT_PAGE1_BASE ((unsigned char *)0x400)
#define TEXT_PAGE2_BASE ((unsigned char *)0x800)

// Standard types.
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed int int16_t;
typedef unsigned int uint16_t;

// Returns non-zero if a key is ready to be read.
extern int keyboard_test(void);

// Clears current key, sets up next one for test or get
extern void keyboard_next(void);

// Wait until a key is ready and then return it without high bit set
extern unsigned char keyboard_get(void);

#endif // __PLATFORM_H__
