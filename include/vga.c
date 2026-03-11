#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t* const vga = (uint16_t*)0xB8000;
static uint8_t current_color = (VGA_WHITE | (VGA_BLACK << 4));

static uint16_t cursor;

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = fg | (bg << 4);
}

void vga_clear(void) {
    uint16_t blank = (current_color << 8) | ' ';

    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }

    cursor = 0;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\r') {
        cursor = (cursor / VGA_WIDTH) * VGA_WIDTH;
    } else {
        vga[cursor++] = (current_color << 8) | c;
    }

    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        cursor = 0; // simples por enquanto
    }
}

void vga_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        vga_putchar(s[i]);
    }
}
