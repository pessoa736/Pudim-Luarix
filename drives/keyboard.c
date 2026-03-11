#include "keyboard.h"
#include "utypes.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_STATUS_OUTPUT_FULL 0x01

static int g_shift_pressed = 0;

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

int keyboard_getchar_nonblock(char* out) {
    uint8_t status;
    uint8_t scancode;
    char c;

    if (!out) {
        return 0;
    }

    status = kbd_inb(KBD_STATUS_PORT);
    if ((status & KBD_STATUS_OUTPUT_FULL) == 0) {
        return 0;
    }

    scancode = kbd_inb(KBD_DATA_PORT);

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

    c = kbd_map_scancode(scancode, g_shift_pressed);
    if (!c) {
        return 0;
    }

    *out = c;
    return 1;
}
