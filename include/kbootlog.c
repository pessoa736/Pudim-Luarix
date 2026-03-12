#include "kbootlog.h"
#include "kio.h"
#include "kfs.h"
#include "serial.h"
#include "vga.h"

static int g_bootlog_file_enabled = 0;

void kbootlog_enable_file(void) {
    g_bootlog_file_enabled = 1;
    kfs_write("boot.log", "");
}

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

    if (g_bootlog_file_enabled) {
        kfs_append("boot.log", "[");
        kfs_append("boot.log", title);
        kfs_append("boot.log", "] ");
    }
}

void kbootlog_write(const char* msg) {
    if (!msg) {
        return;
    }

    kio_write(msg);
    serial_print(msg);

    if (g_bootlog_file_enabled) {
        kfs_append("boot.log", msg);
    }
}

void kbootlog_writeln(const char* msg) {
    if (msg) {
        kbootlog_write(msg);
    }

    kio_write("\n");
    serial_print("\r\n");

    if (g_bootlog_file_enabled) {
        kfs_append("boot.log", "\n");
    }
}

void kbootlog_line(const char* title, const char* msg) {
    kbootlog_title(title);
    kbootlog_writeln(msg);
}