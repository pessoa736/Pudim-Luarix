#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Special key codes returned in high byte range (not valid ASCII) */
#define KBD_KEY_UP      0x80
#define KBD_KEY_DOWN    0x81
#define KBD_KEY_LEFT    0x82
#define KBD_KEY_RIGHT   0x83
#define KBD_KEY_HOME    0x84
#define KBD_KEY_END     0x85
#define KBD_KEY_DELETE  0x86

int keyboard_getchar_nonblock(char* out);
int keyboard_getkey_nonblock(unsigned char* out);
int keyboard_available(void);

#endif
