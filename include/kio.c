#include "kio.h"
#include "vga.h"

void kio_write(const char* s) {
    if (!s) {
        return;
    }

    vga_print(s);
}

void kio_writeln(const char* s) {
    if (s) {
        vga_print(s);
    }

    vga_print("\n");
}

void kio_clear(void) {
    vga_clear();
}
