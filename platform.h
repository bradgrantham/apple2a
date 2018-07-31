#define TEXT_PAGE1_BASE ((volatile unsigned char *)0x400)
#define TEXT_PAGE2_BASE ((volatile unsigned char *)0x800)

// Returns non-zero if a key is ready to be read.
extern int keyboard_test(void);

// Clears current key, sets up next one for test or get
extern void keyboard_next(void);

// Wait until a key is ready and then return it without high bit set
extern unsigned char keyboard_get(void);
