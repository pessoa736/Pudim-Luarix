#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5

static volatile uint16_t* const vga = (uint16_t*)0xB8000;
static uint8_t current_color = (VGA_WHITE | (VGA_BLACK << 4));

static uint16_t cursor;

static inline void vga_outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "d"(port));
}

static void vga_update_hw_cursor(void) {
    vga_outb(VGA_CRTC_INDEX, 0x0F);
    vga_outb(VGA_CRTC_DATA, (uint8_t)(cursor & 0xFF));
    vga_outb(VGA_CRTC_INDEX, 0x0E);
    vga_outb(VGA_CRTC_DATA, (uint8_t)((cursor >> 8) & 0xFF));
}

static void vga_scroll_up_one(void) {
    uint16_t blank = (current_color << 8) | ' ';
    uint16_t total = VGA_WIDTH * VGA_HEIGHT;
    uint16_t last_row_start = total - VGA_WIDTH;
    uint16_t i;

    for (i = 0; i < last_row_start; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }

    for (i = last_row_start; i < total; i++) {
        vga[i] = blank;
    }
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = fg | (bg << 4);
}

void vga_clear(void) {
    uint16_t blank = (current_color << 8) | ' ';

    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }

    cursor = 0;
    vga_update_hw_cursor();
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            vga[cursor] = (current_color << 8) | ' ';
        }
    } else if (c == '\r') {
        cursor = (cursor / VGA_WIDTH) * VGA_WIDTH;
    } else {
        vga[cursor++] = (current_color << 8) | c;
    }

    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        vga_scroll_up_one();
        cursor -= VGA_WIDTH;
    }

    vga_update_hw_cursor();
}

void vga_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        vga_putchar(s[i]);
    }
}
