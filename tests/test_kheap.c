/* Unit tests for kheap.c */
#include "test_framework.h"
#include <stdint.h>

/*
 * Include utypes/stddef first so kheap.c gets the kernel types.
 * kheap.c uses asm volatile("lock cmpxchgl ...") which works
 * natively on the host x86_64, so no mocking needed for the spinlock.
 */
#include "../include/utypes.h"
#include "../include/stddef.h"

/* Provide a fake __kernel_end symbol */
uint8_t __kernel_end;

/* Include the kheap source directly */
#include "../include/kheap.c"

/* Override heap_init to use a static buffer instead of __kernel_end address */
static char fake_heap[0x200000] __attribute__((aligned(4096)));

static void test_heap_init(void) {
    heap_region_start = (size_t)fake_heap;
    heap_region_end = (size_t)fake_heap + 0x100000;
    heap_start = (block_header_t*)fake_heap;
    heap_start->size = (0x100000 - sizeof(block_header_t)) & ~(HEAP_ALIGNMENT - 1);
    heap_start->free = 1;
    heap_start->magic = BLOCK_MAGIC_FREE;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    g_heap_lock = 0;
}

static void test_basic_alloc(void) {
    void* p;

    TEST_SUITE("kheap: basic allocation");
    test_heap_init();

    p = kmalloc(64);
    ASSERT_NOT_NULL(p, "kmalloc(64) returns non-null");

    kfree(p);
}

static void test_multiple_alloc(void) {
    void* a;
    void* b;
    void* c;

    TEST_SUITE("kheap: multiple allocations");
    test_heap_init();

    a = kmalloc(32);
    b = kmalloc(64);
    c = kmalloc(128);

    ASSERT_NOT_NULL(a, "alloc a");
    ASSERT_NOT_NULL(b, "alloc b");
    ASSERT_NOT_NULL(c, "alloc c");
    ASSERT(a != b, "a != b");
    ASSERT(b != c, "b != c");

    kfree(a);
    kfree(b);
    kfree(c);
}

static void test_free_reuse(void) {
    void* a;
    void* b;
    void* c;

    TEST_SUITE("kheap: free and reuse");
    test_heap_init();

    a = kmalloc(64);
    b = kmalloc(64);
    kfree(a);
    c = kmalloc(64);

    ASSERT_NOT_NULL(c, "realloc after free");
    /* c should reuse a's slot */
    ASSERT_EQ(c, a, "freed block is reused");

    kfree(b);
    kfree(c);
}

static void test_krealloc(void) {
    void* p;
    void* p2;

    TEST_SUITE("kheap: krealloc");
    test_heap_init();

    p = kmalloc(32);
    ASSERT_NOT_NULL(p, "initial alloc");

    /* Write data */
    memset(p, 'A', 32);

    p2 = krealloc(p, 128);
    ASSERT_NOT_NULL(p2, "krealloc returns non-null");
    /* Data should be preserved */
    ASSERT_EQ(((char*)p2)[0], 'A', "data preserved after realloc");
    ASSERT_EQ(((char*)p2)[31], 'A', "data end preserved");

    kfree(p2);
}

static void test_null_alloc(void) {
    TEST_SUITE("kheap: edge cases");
    test_heap_init();

    ASSERT_NULL(kmalloc(0), "kmalloc(0) returns NULL");
}

static void test_free_bytes(void) {
    void* p;
    size_t before;
    size_t after;

    TEST_SUITE("kheap: free/total bytes");
    test_heap_init();

    before = kheap_free_bytes();
    ASSERT(before > 0, "initial free > 0");
    ASSERT(kheap_total_bytes() > 0, "total > 0");

    p = kmalloc(1024);
    after = kheap_free_bytes();
    ASSERT(after < before, "free decreases after alloc");

    kfree(p);
}

static void test_merge_blocks(void) {
    void* a;
    void* b;
    void* c;
    void* big;

    TEST_SUITE("kheap: block merging");
    test_heap_init();

    a = kmalloc(32);
    b = kmalloc(32);
    c = kmalloc(32);

    kfree(a);
    kfree(b);
    kfree(c);

    /* After freeing all three adjacent blocks, they should merge.
       We should be able to allocate a large block. */
    big = kmalloc(80);
    ASSERT_NOT_NULL(big, "large alloc after merge");

    kfree(big);
}

int main(void) {
    test_basic_alloc();
    test_multiple_alloc();
    test_free_reuse();
    test_krealloc();
    test_null_alloc();
    test_free_bytes();
    test_merge_blocks();
    TEST_RESULTS();
}
