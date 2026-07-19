#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <3ds.h>

void keyboard_draw(void);

/**
 * Get character from touch position on lower screen.
 * Returns 0 if no valid key was pressed.
 * Returns 8 for backspace, 13 for send/enter.
 */
char keyboard_get_char(int touch_x, int touch_y);

void keyboard_reset(void);

#endif // KEYBOARD_H
