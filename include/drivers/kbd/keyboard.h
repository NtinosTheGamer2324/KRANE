#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_ESC   27
#define KEY_UP    ((char)0x80)
#define KEY_DOWN  ((char)0x81)
#define KEY_ENTER '\n'

void keyboard_init(void);
char keyboard_get_char(void);

#endif
