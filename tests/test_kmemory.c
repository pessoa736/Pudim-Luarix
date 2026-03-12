/* Unit tests for kmemory.c (page pool allocator, no actual paging) */
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/*
 * kmemory.c uses asm volatile for invlpg, cr3, etc.
 * We neutralize all asm volatile by redefining it as a no-op,
 * and wrap the page directory pointer to a local buffer.
 */

/* Redirect asm volatile to suppress all inline assembly */
#define asm(...)

/* Redirect the page directory base address to a local buffer */
#define KMEMORY_PD_BASE_ADDR ((uint64_t)(size_t)fake_pd_storage)
static uint64_t fake_pd_storage[512] __attribute__((aligned(4096)));

/* Make kmemory_apply_protection_4k always succeed (skip hw ops) */
#define KMEMORY_TEST_SKIP_PROTECTION

/* Include kmemory.c — asm is now no-op */
#include "../include/kmemory.c"

/* Undef our override */
#undef asm

/* Override kmemory_apply_protection_4k since it walks page tables */
/* We already have the real one compiled above but it won't do real hw ops
   since asm is no-op. However, pd_base returns our fake buffer which has
   no valid PTEs. The function will return 0 from the pde check.
   Instead, let's test only the pool allocator logic. */

static void test_init(void) {
    TEST_SUITE("kmemory: init");
    kmemory_init();

    ASSERT_EQ(kmemory_total_allocated(), 0, "0 allocated after init");
    ASSERT_EQ(kmemory_block_count(), 0, "0 blocks after init");
}

static void test_pool_find_span(void) {
    uint32_t start;

    TEST_SUITE("kmemory: pool_find_span");
    kmemory_init();

    /* All pages free, should find span of 1 at start */
    ASSERT_EQ(kmemory_pool_find_span(1, &start), 1, "find 1 page");
    ASSERT_EQ(start, 0, "starts at 0");

    /* Mark some pages used, then find */
    g_kmemory_page_used[0] = 1;
    g_kmemory_page_used[1] = 1;
    ASSERT_EQ(kmemory_pool_find_span(1, &start), 1, "find after skip");
    ASSERT_EQ(start, 2, "starts at 2");

    /* Too large */
    ASSERT_EQ(kmemory_pool_find_span(999, &start), 0, "too many pages");

    /* Null out pointer */
    ASSERT_EQ(kmemory_pool_find_span(1, NULL), 0, "null output");
    ASSERT_EQ(kmemory_pool_find_span(0, &start), 0, "0 pages");
}

static void test_align_up_page(void) {
    TEST_SUITE("kmemory: align_up_page");

    ASSERT_EQ(kmemory_align_up_page(0), 0, "align 0");
    ASSERT_EQ(kmemory_align_up_page(1), 4096, "align 1 -> 4096");
    ASSERT_EQ(kmemory_align_up_page(4096), 4096, "align 4096");
    ASSERT_EQ(kmemory_align_up_page(4097), 8192, "align 4097 -> 8192");
}

static void test_page_pool_alloc_free(void) {
    uint32_t start;

    TEST_SUITE("kmemory: page pool alloc/free cycle");
    kmemory_init();

    /* Simulate allocation: mark 3 pages used */
    g_kmemory_page_used[0] = 1;
    g_kmemory_page_used[1] = 1;
    g_kmemory_page_used[2] = 1;

    /* Find span should skip the first 3 */
    ASSERT_EQ(kmemory_pool_find_span(2, &start), 1, "find 2-page span");
    ASSERT_EQ(start, 3, "starts at page 3");

    /* Free page 1, try again */
    g_kmemory_page_used[1] = 0;
    ASSERT_EQ(kmemory_pool_find_span(1, &start), 1, "find 1-page in gap");
    ASSERT_EQ(start, 1, "reuses freed page 1");
}

static void test_block_tracking(void) {
    TEST_SUITE("kmemory: block tracking");
    kmemory_init();

    /* Manually add a block */
    g_kmemory_blocks[0].ptr = (void*)0x1000;
    g_kmemory_blocks[0].size = 4096;
    g_kmemory_blocks[0].flags = KMEMORY_FLAG_READ | KMEMORY_FLAG_WRITE;
    g_kmemory_blocks[0].used = 1;
    g_kmemory_total = 4096;

    ASSERT_EQ(kmemory_total_allocated(), 4096, "total 4096");
    ASSERT_EQ(kmemory_block_count(), 1, "1 block");
}

static void test_free_null(void) {
    TEST_SUITE("kmemory: free null");
    kmemory_init();

    ASSERT_EQ(kmemory_free(NULL), 0, "free null returns 0");
}

static void test_dump_stats(void) {
    char buf[256];

    TEST_SUITE("kmemory: dump_stats");
    kmemory_init();

    /* Add a block for non-empty stats */
    g_kmemory_blocks[0].ptr = (void*)&g_kmemory_page_pool[0][0];
    g_kmemory_blocks[0].size = 4096;
    g_kmemory_blocks[0].flags = KMEMORY_FLAG_READ;
    g_kmemory_blocks[0].used = 1;
    g_kmemory_total = 4096;

    buf[0] = 0;
    kmemory_dump_stats(buf, sizeof(buf));
    ASSERT(strlen(buf) > 0, "stats not empty");
}

static void test_string_helpers(void) {
    char buf[32];

    TEST_SUITE("kmemory: string helpers");

    ASSERT_EQ(kmemory_strlen("hello"), 5, "strlen");
    ASSERT_EQ(kmemory_strlen(NULL), 0, "strlen null");

    strcpy(buf, "abc");
    kmemory_strcat(buf, sizeof(buf), "def");
    ASSERT(strcmp(buf, "abcdef") == 0, "strcat");

    kmemory_u64_to_str(12345, buf, sizeof(buf));
    ASSERT(strcmp(buf, "12345") == 0, "u64_to_str 12345");

    kmemory_u64_to_str(0, buf, sizeof(buf));
    ASSERT(strcmp(buf, "0") == 0, "u64_to_str 0");
}

int main(void) {
    test_init();
    test_pool_find_span();
    test_align_up_page();
    test_page_pool_alloc_free();
    test_block_tracking();
    test_free_null();
    test_dump_stats();
    test_string_helpers();
    TEST_RESULTS();
}
