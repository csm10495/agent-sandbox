#pragma once
#include "types.h"

void keyboard_init(void);
int  keyboard_getchar(void);   /* blocking: waits for a keypress */
int  keyboard_poll(void);      /* non-blocking: returns char or -1 */
bool keyboard_ctrl_c(void);    /* returns true if Ctrl+C was pressed */
