#include "platform.h"

#define KBD_STATUS_OR_KEY ((unsigned char *)0xC000)
#define KBD_CLEAR_CURRENT_KEY ((unsigned char *)0xC010)

#define KEY_READY_MASK 0x80
#define KEY_VALUE_MASK 0x7F

// Returns non-zero if a key is ready to be read.
int keyboard_test(void)
{
    return *KBD_STATUS_OR_KEY & KEY_READY_MASK;
}

// Clears current key, sets up next one for test or get
void keyboard_clear(void)
{
    unsigned char clear_key;

    // On 6502, a write cycle READS AND THEN WRITES, so just read the
    // "latch next key" strobe.
    clear_key = *KBD_CLEAR_CURRENT_KEY;
}

// Wait until a key is ready and then return it without high bit set
unsigned char keyboard_get(void)
{
    unsigned char key;

    while(!((key = *KBD_STATUS_OR_KEY) & KEY_READY_MASK));
    
    keyboard_clear();

    return key & KEY_VALUE_MASK;
}
