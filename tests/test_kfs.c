/* Unit tests for kfs.c */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/*
 * kfs.c depends on kheap (kmalloc/kfree/krealloc), ata, serial.
 * We include kheap.c directly and stub the rest.
 */

/* Provide __kernel_end for kheap */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* Stub ATA persistence */
int ata_init(void) { return 0; }
int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size; return 0;
}
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size; return 0;
}
int ata_is_available(void) { return 0; }

/* Stub serial */
void serial_print(const char* s) { (void)s; }

/* Include kfs source */
#include "../include/kfs.c"

/* Init heap with a static buffer */
static char fake_heap[0x200000] __attribute__((aligned(4096)));

static void setup(void) {
    heap_region_start = (size_t)fake_heap;
    heap_region_end = (size_t)fake_heap + 0x100000;
    heap_start = (block_header_t*)fake_heap;
    heap_start->size = (0x100000 - sizeof(block_header_t)) & ~(HEAP_ALIGNMENT - 1);
    heap_start->free = 1;
    heap_start->magic = BLOCK_MAGIC_FREE;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    g_heap_lock = 0;
    kfs_init();
}

static void test_write_read(void) {
    const char* data;

    TEST_SUITE("kfs: write and read");
    setup();

    ASSERT_EQ(kfs_write("hello.txt", "world"), 1, "write returns 1");
    ASSERT_EQ(kfs_exists("hello.txt"), 1, "file exists");

    data = kfs_read("hello.txt");
    ASSERT_NOT_NULL(data, "read returns non-null");
    ASSERT_STR_EQ(data, "world", "content matches");
}

static void test_overwrite(void) {
    const char* data;

    TEST_SUITE("kfs: overwrite");
    setup();

    kfs_write("f.txt", "old");
    kfs_write("f.txt", "new");

    data = kfs_read("f.txt");
    ASSERT_NOT_NULL(data, "read after overwrite");
    ASSERT_STR_EQ(data, "new", "content is updated");
    ASSERT_EQ(kfs_count(), 1, "still one file");
}

static void test_append(void) {
    const char* data;

    TEST_SUITE("kfs: append");
    setup();

    kfs_write("log.txt", "line1");
    kfs_append("log.txt", " line2");

    data = kfs_read("log.txt");
    ASSERT_NOT_NULL(data, "read after append");
    ASSERT_STR_EQ(data, "line1 line2", "appended content");
}

static void test_append_creates(void) {
    const char* data;

    TEST_SUITE("kfs: append creates file");
    setup();

    kfs_append("new.txt", "data");

    ASSERT_EQ(kfs_exists("new.txt"), 1, "file created");
    data = kfs_read("new.txt");
    ASSERT_STR_EQ(data, "data", "content from append-create");
}

static void test_delete(void) {
    TEST_SUITE("kfs: delete");
    setup();

    kfs_write("del.txt", "x");
    ASSERT_EQ(kfs_exists("del.txt"), 1, "exists before delete");

    ASSERT_EQ(kfs_delete("del.txt"), 1, "delete returns 1");
    ASSERT_EQ(kfs_exists("del.txt"), 0, "gone after delete");
    ASSERT_NULL(kfs_read("del.txt"), "read returns null");
}

static void test_count_and_name_at(void) {
    TEST_SUITE("kfs: count and name_at");
    setup();

    kfs_write("a.txt", "1");
    kfs_write("b.txt", "2");
    kfs_write("c.txt", "3");

    ASSERT_EQ(kfs_count(), 3, "3 files");
    ASSERT_NOT_NULL(kfs_name_at(0), "name_at(0)");
    ASSERT_NOT_NULL(kfs_name_at(1), "name_at(1)");
    ASSERT_NOT_NULL(kfs_name_at(2), "name_at(2)");
    ASSERT_NULL(kfs_name_at(3), "name_at(3) is null");
}

static void test_size(void) {
    TEST_SUITE("kfs: size");
    setup();

    kfs_write("s.txt", "12345");
    ASSERT_EQ(kfs_size("s.txt"), 5, "size is 5");

    kfs_append("s.txt", "67");
    ASSERT_EQ(kfs_size("s.txt"), 7, "size after append");
}

static void test_invalid_names(void) {
    TEST_SUITE("kfs: invalid names");
    setup();

    ASSERT_EQ(kfs_write(NULL, "x"), 0, "null name rejected");
    ASSERT_EQ(kfs_write("", "x"), 0, "empty name rejected");
    ASSERT_EQ(kfs_read(NULL), NULL, "read null name");
    ASSERT_EQ(kfs_exists(NULL), 0, "exists null name");
    ASSERT_EQ(kfs_delete(NULL), 0, "delete null name");
}

static void test_clear(void) {
    TEST_SUITE("kfs: clear");
    setup();

    kfs_write("a.txt", "1");
    kfs_write("b.txt", "2");
    ASSERT_EQ(kfs_count(), 2, "2 files before clear");

    kfs_clear();
    ASSERT_EQ(kfs_count(), 0, "0 files after clear");
}

int main(void) {
    test_write_read();
    test_overwrite();
    test_append();
    test_append_creates();
    test_delete();
    test_count_and_name_at();
    test_size();
    test_invalid_names();
    test_clear();
    TEST_RESULTS();
}
