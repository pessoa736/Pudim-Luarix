#include "keyboard.h"
#include "utypes.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_STATUS_OUTPUT_FULL 0x01

/* PS/2 scancodes for special keys (set 1) */
#define SC_UP       0x48
#define SC_DOWN     0x50
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D
#define SC_HOME     0x47
#define SC_END      0x4F
#define SC_DELETE   0x53

static int g_shift_pressed = 0;
static int g_e0_prefix = 0;

static inline uint8_t kbd_inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

static char kbd_map_scancode(uint8_t scancode, int shift) {
    static const char map[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
        '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    };

    static const char map_shift[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
        '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    };

    if (scancode >= 128) {
        return 0;
    }

    if (shift) {
        return map_shift[scancode];
    }

    return map[scancode];
}

static unsigned char kbd_map_e0_scancode(uint8_t scancode) {
    switch (scancode) {
        case SC_UP:     return KBD_KEY_UP;
        case SC_DOWN:   return KBD_KEY_DOWN;
        case SC_LEFT:   return KBD_KEY_LEFT;
        case SC_RIGHT:  return KBD_KEY_RIGHT;
        case SC_HOME:   return KBD_KEY_HOME;
        case SC_END:    return KBD_KEY_END;
        case SC_DELETE: return KBD_KEY_DELETE;
        default:        return 0;
    }
}

int keyboard_getkey_nonblock(unsigned char* out) {
    uint8_t status;
    uint8_t scancode;

    if (!out) {
        return 0;
    }

    status = kbd_inb(KBD_STATUS_PORT);
    if ((status & KBD_STATUS_OUTPUT_FULL) == 0) {
        return 0;
    }

    scancode = kbd_inb(KBD_DATA_PORT);

    if (scancode == 0xE0) {
        g_e0_prefix = 1;
        return 0;
    }

    if (g_e0_prefix) {
        g_e0_prefix = 0;

        if (scancode & 0x80) {
            return 0;
        }

        *out = kbd_map_e0_scancode(scancode);
        return *out != 0;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        g_shift_pressed = 1;
        return 0;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        g_shift_pressed = 0;
        return 0;
    }

    if (scancode & 0x80) {
        return 0;
    }

    {
        char c = kbd_map_scancode(scancode, g_shift_pressed);
        if (!c) {
            return 0;
        }
        *out = (unsigned char)c;
        return 1;
    }
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
    uint8_t status = kbd_inb(KBD_STATUS_PORT);
    return (status & KBD_STATUS_OUTPUT_FULL) != 0;
}
