/*
 * keyboard_aarch64.c — Serial-based keyboard input for aarch64
 *
 * On aarch64 there is no PS/2 keyboard controller. Input comes
 * exclusively from the serial port (Termux, minicom, etc.).
 * Translates ANSI escape sequences to key codes.
 */
#include "keyboard.h"
#include "serial.h"
#include "utypes.h"

/* Parse ANSI escape sequences from serial input */
static int g_esc_state = 0;  /* 0=normal, 1=got ESC, 2=got [ */

int keyboard_getkey_nonblock(unsigned char* out) {
    char c;

    if (!out) {
        return 0;
    }

    if (!serial_read_nonblock(&c)) {
        return 0;
    }

    /* ESC sequence handling */
    if (g_esc_state == 0 && c == 0x1B) {
        g_esc_state = 1;
        return 0;
    }

    if (g_esc_state == 1) {
        if (c == '[') {
            g_esc_state = 2;
            return 0;
        }
        /* Not a CSI, return ESC as key 27 */
        g_esc_state = 0;
        *out = 27;
        return 1;
    }

    if (g_esc_state == 2) {
        g_esc_state = 0;
        switch (c) {
            case 'A': *out = KBD_KEY_UP;     return 1;
            case 'B': *out = KBD_KEY_DOWN;   return 1;
            case 'C': *out = KBD_KEY_RIGHT;  return 1;
            case 'D': *out = KBD_KEY_LEFT;   return 1;
            case 'H': *out = KBD_KEY_HOME;   return 1;
            case 'F': *out = KBD_KEY_END;    return 1;
            case '3':
                /* Delete key: ESC[3~ — consume the ~ */
                {
                    char tilde;
                    serial_read_nonblock(&tilde);
                }
                *out = KBD_KEY_DELETE;
                return 1;
            default:
                return 0;
        }
    }

    /* Map CR to Enter */
    if (c == '\r') {
        *out = '\n';
        return 1;
    }

    /* Backspace: DEL (0x7F) or BS (0x08) */
    if (c == 0x7F || c == 0x08) {
        *out = 127;
        return 1;
    }

    *out = (unsigned char)c;
    return 1;
}

int keyboard_getchar_nonblock(char* out) {
    unsigned char key;

    if (!out) {
        return 0;
    }

    if (!keyboard_getkey_nonblock(&key)) {
        return 0;
    }

    if (key >= 0x80) {
        return 0;
    }

    *out = (char)key;
    return 1;
}

int keyboard_available(void) {
    return serial_can_read();
}
