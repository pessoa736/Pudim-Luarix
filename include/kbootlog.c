#include "kbootlog.h"
#include "kio.h"
#include "serial.h"
#include "vga.h"

void kbootlog_title(const char* title) {
    if (!title) {
        return;
    }

    vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    kio_write("[");
    kio_write(title);
    kio_write("] ");
    vga_set_color(VGA_WHITE, VGA_BLACK);

    serial_print("[");
    serial_print(title);
    serial_print("] ");
}

void kbootlog_write(const char* msg) {
    if (!msg) {
        return;
    }

    kio_write(msg);
    serial_print(msg);
}

void kbootlog_writeln(const char* msg) {
    if (msg) {
        kbootlog_write(msg);
    }

    kio_write("\n");
    serial_print("\r\n");
}

void kbootlog_line(const char* title, const char* msg) {
    kbootlog_title(title);
    kbootlog_writeln(msg);
}