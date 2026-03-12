/*
 * Hardware mocks for host-based unit testing.
 * Stubs out VGA, serial, ATA, and provides __kernel_end + heap backing.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* --- Fake __kernel_end for kheap --- */
static char fake_heap_region[0x200000] __attribute__((aligned(4096)));
uint8_t __kernel_end __attribute__((section(".data")));

/* Force __kernel_end to sit right before our fake heap */
void mock_heap_setup(void) {
    extern uint8_t __kernel_end;
    /* We can't move __kernel_end, but kheap uses its address.
     * We'll patch heap_init in the test instead. */
}

/* --- VGA stubs --- */
void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }
void vga_clear(void) {}
void vga_putchar(char c) { (void)c; }
void vga_print(const char* s) { (void)s; }
uint16_t vga_get_cursor(void) { return 0; }
void vga_set_cursor(uint16_t pos) { (void)pos; }
void vga_put_at(uint16_t pos, char c) { (void)pos; (void)c; }
char vga_char_at(uint16_t pos) { (void)pos; return ' '; }
void vga_scroll_view_up(int lines) { (void)lines; }
void vga_scroll_view_down(int lines) { (void)lines; }
int vga_is_scrolled_back(void) { return 0; }

/* --- Serial stubs --- */
void serial_init(void) {}
void serial_init_irq(void) {}
void serial_putchar(char c) { (void)c; }
void serial_print(const char* s) { (void)s; }
int serial_can_read(void) { return 0; }
char serial_getchar(void) { return 0; }
int serial_read_nonblock(char* out) { (void)out; return 0; }
void serial_irq_handler(void) {}

/* --- ATA stubs --- */
int ata_init(void) { return 0; }
int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size;
    return 0;
}
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size;
    return 0;
}
int ata_is_available(void) { return 0; }

/* --- Mouse stubs --- */
void mouse_init(void) {}
void mouse_irq_handler(void) {}
int mouse_get_scroll(void) { return 0; }

/* --- Keyboard stubs --- */
int keyboard_getchar_nonblock(char* out) { (void)out; return 0; }
int keyboard_getkey_nonblock(unsigned char* out) { (void)out; return 0; }
int keyboard_available(void) { return 0; }

/* --- KIO stubs (used by kbootlog tests that don't include kio.c) --- */
#ifdef MOCK_KIO
void kio_write(const char* s) { (void)s; }
void kio_writeln(const char* s) { (void)s; }
void kio_clear(void) {}
#endif
