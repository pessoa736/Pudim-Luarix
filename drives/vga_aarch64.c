/*
 * vga_aarch64.c — VGA stubs for aarch64
 *
 * aarch64 has no VGA text mode. All functions are no-ops
 * or return sensible defaults. Output goes through serial.
 */
#include "vga.h"
#include "utypes.h"

void vga_clear(void) {}
void vga_print(const char* s) { (void)s; }
void vga_putchar(char c) { (void)c; }
void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }
uint16_t vga_get_cursor(void) { return 0; }
void vga_set_cursor(uint16_t pos) { (void)pos; }
void vga_put_at(uint16_t pos, char c) { (void)pos; (void)c; }
void vga_scroll_view_up(int lines) { (void)lines; }
void vga_scroll_view_down(int lines) { (void)lines; }
int  vga_scroll_offset(void) { return 0; }
int  vga_is_scrolled_back(void) { return 0; }
