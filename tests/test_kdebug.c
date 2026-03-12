/* Unit tests for kdebug.c — pudding layers, baking timer, ingredient check */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/* Provide __kernel_end for kheap */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* Stub ksys_uptime_ms for baking timer tests */
static uint64_t g_fake_uptime_ms = 0;

uint64_t ksys_uptime_ms(void) {
    return g_fake_uptime_ms;
}

/* Include source under test */
#include "../include/kdebug.c"

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

    g_fake_uptime_ms = 0;
    kdebug_history_init();
    kdebug_timer_reset();
}

/* --- Pudding Layers: history tests --- */

static void test_history_empty(void) {
    TEST_SUITE("history empty");
    setup();

    ASSERT_EQ(kdebug_history_count(), 0u, "count is 0 initially");
    ASSERT_NULL(kdebug_history_at(0), "at(0) returns NULL when empty");
}

static void test_history_push_one(void) {
    TEST_SUITE("history push one");
    setup();

    kdebug_history_push("ls");
    ASSERT_EQ(kdebug_history_count(), 1u, "count is 1 after one push");

    {
        const char* entry = kdebug_history_at(0);
        ASSERT_NOT_NULL(entry, "at(0) is not null");
        ASSERT_STR_EQ(entry, "ls", "at(0) is 'ls'");
    }
}

static void test_history_push_multiple(void) {
    TEST_SUITE("history push multiple");
    setup();

    kdebug_history_push("ls");
    kdebug_history_push("ps");
    kdebug_history_push("help");

    ASSERT_EQ(kdebug_history_count(), 3u, "count is 3");
    ASSERT_STR_EQ(kdebug_history_at(0), "help", "at(0) = most recent");
    ASSERT_STR_EQ(kdebug_history_at(1), "ps", "at(1) = second most recent");
    ASSERT_STR_EQ(kdebug_history_at(2), "ls", "at(2) = oldest");
    ASSERT_NULL(kdebug_history_at(3), "at(3) returns NULL");
}

static void test_history_skip_empty(void) {
    TEST_SUITE("history skip empty");
    setup();

    kdebug_history_push("");
    ASSERT_EQ(kdebug_history_count(), 0u, "empty string not pushed");

    kdebug_history_push(NULL);
    ASSERT_EQ(kdebug_history_count(), 0u, "NULL not pushed");
}

static void test_history_skip_duplicate(void) {
    TEST_SUITE("history skip duplicate");
    setup();

    kdebug_history_push("ls");
    kdebug_history_push("ls");
    ASSERT_EQ(kdebug_history_count(), 1u, "duplicate not pushed");

    kdebug_history_push("ps");
    kdebug_history_push("ls");
    ASSERT_EQ(kdebug_history_count(), 3u, "non-consecutive duplicate pushed");
}

static void test_history_ring_overflow(void) {
    unsigned int i;
    char buf[16];

    TEST_SUITE("history ring overflow");
    setup();

    /* Push more than KDEBUG_MAX_LAYERS to test ring buffer */
    for (i = 0; i < KDEBUG_MAX_LAYERS + 5; i++) {
        buf[0] = 'a' + (char)(i % 26);
        buf[1] = '0' + (char)(i / 26);
        buf[2] = 0;
        kdebug_history_push(buf);
    }

    ASSERT_EQ(kdebug_history_count(), KDEBUG_MAX_LAYERS, "count capped at max");

    /* Most recent should be the last pushed */
    {
        unsigned int last_i = KDEBUG_MAX_LAYERS + 5 - 1;
        buf[0] = 'a' + (char)(last_i % 26);
        buf[1] = '0' + (char)(last_i / 26);
        buf[2] = 0;
        ASSERT_STR_EQ(kdebug_history_at(0), buf, "most recent correct after overflow");
    }
}

/* --- Baking Timer: profiler tests --- */

static void test_timer_basic(void) {
    uint64_t elapsed;

    TEST_SUITE("baking timer basic");
    setup();

    g_fake_uptime_ms = 1000;
    kdebug_timer_start();

    g_fake_uptime_ms = 1500;
    elapsed = kdebug_timer_stop();

    ASSERT_EQ(elapsed, 500u, "elapsed is 500ms");
}

static void test_timer_not_running(void) {
    TEST_SUITE("baking timer not running");
    setup();

    ASSERT_EQ(kdebug_timer_stop(), 0u, "stop without start returns 0");
}

static void test_timer_marks(void) {
    TEST_SUITE("baking timer marks");
    setup();

    g_fake_uptime_ms = 0;
    kdebug_timer_start();

    g_fake_uptime_ms = 100;
    ASSERT(kdebug_timer_mark("caramel"), "mark caramel succeeds");

    g_fake_uptime_ms = 250;
    ASSERT(kdebug_timer_mark("custard"), "mark custard succeeds");

    ASSERT_EQ(kdebug_timer_mark_count(), 2u, "2 marks recorded");

    ASSERT_STR_EQ(kdebug_timer_mark_label(0), "caramel", "label 0 = caramel");
    ASSERT_EQ(kdebug_timer_mark_ms(0), 100u, "mark 0 at 100ms");

    ASSERT_STR_EQ(kdebug_timer_mark_label(1), "custard", "label 1 = custard");
    ASSERT_EQ(kdebug_timer_mark_ms(1), 250u, "mark 1 at 250ms");
}

static void test_timer_max_marks(void) {
    unsigned int i;

    TEST_SUITE("baking timer max marks");
    setup();

    g_fake_uptime_ms = 0;
    kdebug_timer_start();

    for (i = 0; i < KDEBUG_MAX_MARKS; i++) {
        g_fake_uptime_ms = (i + 1) * 10;
        ASSERT(kdebug_timer_mark("x"), "mark in range succeeds");
    }

    g_fake_uptime_ms = 9999;
    ASSERT(!kdebug_timer_mark("overflow"), "mark beyond max fails");
    ASSERT_EQ(kdebug_timer_mark_count(), KDEBUG_MAX_MARKS, "count = max");
}

static void test_timer_reset_clears(void) {
    TEST_SUITE("baking timer reset");
    setup();

    g_fake_uptime_ms = 0;
    kdebug_timer_start();
    g_fake_uptime_ms = 100;
    kdebug_timer_mark("x");

    kdebug_timer_reset();
    ASSERT_EQ(kdebug_timer_mark_count(), 0u, "marks cleared after reset");
    ASSERT_EQ(kdebug_timer_stop(), 0u, "not running after reset");
}

/* --- Ingredient Check: heap inspector tests --- */

static void test_inspect_initial_heap(void) {
    TEST_SUITE("ingredient check initial heap");
    setup();

    ASSERT_EQ(kdebug_heap_block_count(), 1u, "1 block initially");
    ASSERT_EQ(kdebug_heap_free_block_count(), 1u, "1 free block initially");
    ASSERT(kdebug_heap_largest_free() > 0, "largest free > 0");
}

static void test_inspect_after_alloc(void) {
    void* a;
    void* b;

    TEST_SUITE("ingredient check after alloc");
    setup();

    a = kmalloc(64);
    b = kmalloc(128);

    ASSERT_NOT_NULL(a, "alloc a ok");
    ASSERT_NOT_NULL(b, "alloc b ok");

    ASSERT(kdebug_heap_block_count() >= 3u, "at least 3 blocks after 2 allocs");
    ASSERT(kdebug_heap_free_block_count() >= 1u, "at least 1 free block");

    kfree(b);
    {
        unsigned int free_after = kdebug_heap_free_block_count();
        ASSERT(free_after >= 1u, "still has free blocks after partial free");
    }

    kfree(a);
}

static void test_inspect_fragmentation(void) {
    void* a;
    void* b;
    void* c;

    TEST_SUITE("ingredient check fragmentation");
    setup();

    /* Allocate 3 blocks, free the middle one to create fragmentation */
    a = kmalloc(32);
    b = kmalloc(32);
    c = kmalloc(32);

    ASSERT_NOT_NULL(a, "alloc a");
    ASSERT_NOT_NULL(b, "alloc b");
    ASSERT_NOT_NULL(c, "alloc c");

    kfree(b);  /* creates a free block between two allocated ones */

    {
        unsigned int free_blocks = kdebug_heap_free_block_count();
        unsigned int total_blocks = kdebug_heap_block_count();
        ASSERT(free_blocks >= 1u, "has free blocks");
        ASSERT(total_blocks >= 3u, "has multiple blocks");
    }

    kfree(a);
    kfree(c);
}

/* --- Main --- */

int main(void) {
    /* Pudding layers */
    test_history_empty();
    test_history_push_one();
    test_history_push_multiple();
    test_history_skip_empty();
    test_history_skip_duplicate();
    test_history_ring_overflow();

    /* Baking timer */
    test_timer_basic();
    test_timer_not_running();
    test_timer_marks();
    test_timer_max_marks();
    test_timer_reset_clears();

    /* Ingredient check */
    test_inspect_initial_heap();
    test_inspect_after_alloc();
    test_inspect_fragmentation();

    TEST_RESULTS();
}
