/* Unit tests for kio.c */
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/* --- VGA capture --- */
static char vga_buf[4096];
static int vga_pos = 0;
static int vga_clear_called = 0;

void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }
void vga_clear(void) { vga_clear_called = 1; }
void vga_print(const char* s) {
    if (!s) return;
    while (*s && vga_pos < (int)sizeof(vga_buf) - 1) {
        vga_buf[vga_pos++] = *s++;
    }
    vga_buf[vga_pos] = 0;
}

static void clear_bufs(void) {
    vga_pos = 0; vga_buf[0] = 0;
    vga_clear_called = 0;
}

/* Include kio source */
#include "../include/kio.c"

static void test_write(void) {
    TEST_SUITE("kio: write");
    clear_bufs();

    kio_write("hello");
    ASSERT_STR_EQ(vga_buf, "hello", "writes to VGA");
}

static void test_write_null(void) {
    TEST_SUITE("kio: write null");
    clear_bufs();

    kio_write(NULL);
    ASSERT_EQ(vga_pos, 0, "null does nothing");
}

static void test_writeln(void) {
    TEST_SUITE("kio: writeln");
    clear_bufs();

    kio_writeln("line");
    ASSERT(strstr(vga_buf, "line") != NULL, "has content");
    ASSERT(strstr(vga_buf, "\n") != NULL, "has newline");
}

static void test_writeln_null(void) {
    TEST_SUITE("kio: writeln null");
    clear_bufs();

    kio_writeln(NULL);
    ASSERT_STR_EQ(vga_buf, "\n", "just newline");
}

static void test_clear(void) {
    TEST_SUITE("kio: clear");
    clear_bufs();

    kio_clear();
    ASSERT_EQ(vga_clear_called, 1, "calls vga_clear");
}

int main(void) {
    test_write();
    test_write_null();
    test_writeln();
    test_writeln_null();
    test_clear();
    TEST_RESULTS();
}
