/* Unit tests for kbootlog.c */
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/*
 * kbootlog.c depends on: kio, kfs, serial, vga.
 * We capture output by replacing VGA/serial/kfs with recording stubs.
 */

/* --- Capture buffers --- */
static char vga_buf[4096];
static int vga_pos = 0;

static char serial_buf[4096];
static int serial_pos = 0;

static char file_buf[4096];
static int file_pos = 0;

static void clear_bufs(void) {
    vga_pos = 0; vga_buf[0] = 0;
    serial_pos = 0; serial_buf[0] = 0;
    file_pos = 0; file_buf[0] = 0;
}

/* --- VGA stubs --- */
void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }
void vga_clear(void) {}
void vga_print(const char* s) {
    if (!s) return;
    while (*s && vga_pos < (int)sizeof(vga_buf) - 1) {
        vga_buf[vga_pos++] = *s++;
    }
    vga_buf[vga_pos] = 0;
}

/* --- Serial stubs --- */
void serial_print(const char* s) {
    if (!s) return;
    while (*s && serial_pos < (int)sizeof(serial_buf) - 1) {
        serial_buf[serial_pos++] = *s++;
    }
    serial_buf[serial_pos] = 0;
}

/* --- KFS stubs (capture appends to boot.log) --- */
int kfs_write(const char* name, const char* content) {
    (void)name;
    file_pos = 0;
    if (content) {
        while (*content && file_pos < (int)sizeof(file_buf) - 1) {
            file_buf[file_pos++] = *content++;
        }
    }
    file_buf[file_pos] = 0;
    return 1;
}

int kfs_append(const char* name, const char* content) {
    (void)name;
    if (!content) return 1;
    while (*content && file_pos < (int)sizeof(file_buf) - 1) {
        file_buf[file_pos++] = *content++;
    }
    file_buf[file_pos] = 0;
    return 1;
}

/* kio_write/kio_writeln call vga_print under the hood; inline them */
#include "../include/kio.c"

/* Include kbootlog source */
#include "../include/kbootlog.c"

static void test_title(void) {
    TEST_SUITE("kbootlog: title");
    clear_bufs();

    kbootlog_title("BOOT");
    ASSERT(strstr(vga_buf, "[BOOT] ") != NULL, "VGA has title");
    ASSERT(strstr(serial_buf, "[BOOT] ") != NULL, "serial has title");
}

static void test_write(void) {
    TEST_SUITE("kbootlog: write");
    clear_bufs();

    kbootlog_write("hello");
    ASSERT_STR_EQ(vga_buf, "hello", "VGA output");
    ASSERT_STR_EQ(serial_buf, "hello", "serial output");
}

static void test_writeln(void) {
    TEST_SUITE("kbootlog: writeln");
    clear_bufs();

    kbootlog_writeln("msg");
    ASSERT(strstr(vga_buf, "msg") != NULL, "VGA has msg");
    ASSERT(strstr(vga_buf, "\n") != NULL, "VGA has newline");
    ASSERT(strstr(serial_buf, "\r\n") != NULL, "serial has CRLF");
}

static void test_line(void) {
    TEST_SUITE("kbootlog: line");
    clear_bufs();

    kbootlog_line("FS", "ok");
    ASSERT(strstr(vga_buf, "[FS] ok\n") != NULL, "VGA line output");
    ASSERT(strstr(serial_buf, "[FS] ok\r\n") != NULL, "serial line output");
}

static void test_file_logging(void) {
    TEST_SUITE("kbootlog: file logging");
    clear_bufs();

    kbootlog_enable_file();
    kbootlog_line("HEAP", "ready");

    ASSERT(strstr(file_buf, "[HEAP] ready\n") != NULL, "boot.log has line");
}

static void test_null_safety(void) {
    TEST_SUITE("kbootlog: null safety");
    clear_bufs();

    /* Should not crash */
    kbootlog_title(NULL);
    kbootlog_write(NULL);
    kbootlog_writeln(NULL);
}

int main(void) {
    test_title();
    test_write();
    test_writeln();
    test_line();
    test_file_logging();
    test_null_safety();
    TEST_RESULTS();
}
