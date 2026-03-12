#include "kbootlog.h"
#include "kio.h"
#include "kfs.h"
#include "serial.h"
#include "kdisplay.h"
#include "arch.h"

#if ARCH_HAS_VGA
#include "vga.h"
#endif

static int g_bootlog_file_enabled = 0;

void kbootlog_enable_file(void) {
    g_bootlog_file_enabled = 1;
    kfs_write("boot.log", "");
}

void kbootlog_title(const char* title) {
    if (!title) {
        return;
    }

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    }
#endif

    kio_write("[");
    kio_write(title);
    kio_write("] ");

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        vga_set_color(VGA_WHITE, VGA_BLACK);
    }
#endif

    /* Mirror to serial only when VGA is primary */
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print("[");
        serial_print(title);
        serial_print("] ");
    }

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

    /* Mirror to serial only when VGA is primary (avoid double print) */
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print(msg);
    }

    if (g_bootlog_file_enabled) {
        kfs_append("boot.log", msg);
    }
}

void kbootlog_writeln(const char* msg) {
    if (msg) {
        kbootlog_write(msg);
    }

    kio_write("\n");

    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print("\r\n");
    }

    if (g_bootlog_file_enabled) {
        kfs_append("boot.log", "\n");
    }
}

void kbootlog_line(const char* title, const char* msg) {
    kbootlog_title(title);
    kbootlog_writeln(msg);
}