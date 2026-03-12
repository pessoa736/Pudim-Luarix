#ifndef VGA_H
#define VGA_H

#include "utypes.h"

enum vga_color {
    VGA_BLACK = 0x0,
    VGA_BLUE = 0x1,
    VGA_GREEN = 0x2,
    VGA_CYAN = 0x3,
    VGA_RED = 0x4,
    VGA_MAGENTA = 0x5,
    VGA_BROWN = 0x6,
    VGA_LIGHT_GREY = 0x7,
    VGA_DARK_GREY = 0x8,
    VGA_LIGHT_BLUE = 0x9,
    VGA_LIGHT_GREEN = 0xA,
    VGA_LIGHT_CYAN = 0xB,
    VGA_LIGHT_RED = 0xC,
    VGA_LIGHT_MAGENTA = 0xD,
    VGA_YELLOW = 0xE,
    VGA_WHITE = 0xF,
};

void vga_set_color(uint8_t fg, uint8_t bg);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char* s);
uint16_t vga_get_cursor(void);
void vga_set_cursor(uint16_t pos);
void vga_put_at(uint16_t pos, char c);
char vga_char_at(uint16_t pos);
void vga_scroll_view_up(int lines);
void vga_scroll_view_down(int lines);
int vga_is_scrolled_back(void);

#endif
