/* Unit tests for ksync.c */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/*
 * ksync.c uses kmalloc/kfree from kheap.
 * Include kheap.c to provide a working allocator.
 */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* Include ksync source (its asm lock cmpxchgl works on host x86_64) */
#include "../include/ksync.c"

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
}

static void test_spinlock_create_destroy(void) {
    ksync_spinlock_t* lock;

    TEST_SUITE("ksync: spinlock create/destroy");
    setup();

    lock = ksync_spinlock_create();
    ASSERT_NOT_NULL(lock, "create returns non-null");
    ASSERT_EQ(lock->locked, 0, "initially unlocked");

    ksync_spinlock_destroy(lock);
}

static void test_spinlock_lock_unlock(void) {
    ksync_spinlock_t* lock;

    TEST_SUITE("ksync: spinlock lock/unlock");
    setup();

    lock = ksync_spinlock_create();
    ASSERT_NOT_NULL(lock, "created");

    ksync_spinlock_lock(lock);
    ASSERT_EQ(lock->locked, 1, "locked after lock");

    ksync_spinlock_unlock(lock);
    ASSERT_EQ(lock->locked, 0, "unlocked after unlock");

    ksync_spinlock_destroy(lock);
}

static void test_spinlock_null_safe(void) {
    TEST_SUITE("ksync: spinlock null safety");
    setup();

    /* Should not crash */
    ksync_spinlock_lock(NULL);
    ksync_spinlock_unlock(NULL);
    ksync_spinlock_destroy(NULL);
}

static void test_mutex_create_destroy(void) {
    ksync_mutex_t* mtx;

    TEST_SUITE("ksync: mutex create/destroy");
    setup();

    mtx = ksync_mutex_create();
    ASSERT_NOT_NULL(mtx, "create returns non-null");
    ASSERT_EQ(mtx->owner_pid, 0, "no owner");
    ASSERT_EQ(mtx->count, 0, "count 0");

    ksync_mutex_destroy(mtx);
}

static void test_mutex_lock_unlock(void) {
    ksync_mutex_t* mtx;

    TEST_SUITE("ksync: mutex lock/unlock");
    setup();

    mtx = ksync_mutex_create();
    ASSERT_NOT_NULL(mtx, "created");

    ksync_mutex_lock(mtx);
    ASSERT_EQ(mtx->count, 1, "count 1 after lock");
    ASSERT_EQ(mtx->owner_pid, 1, "owner set");

    ksync_mutex_unlock(mtx);
    ASSERT_EQ(mtx->count, 0, "count 0 after unlock");
    ASSERT_EQ(mtx->owner_pid, 0, "owner cleared");

    ksync_mutex_destroy(mtx);
}

static void test_mutex_null_safe(void) {
    TEST_SUITE("ksync: mutex null safety");
    setup();

    ksync_mutex_lock(NULL);
    ksync_mutex_unlock(NULL);
    ksync_mutex_destroy(NULL);
}

int main(void) {
    test_spinlock_create_destroy();
    test_spinlock_lock_unlock();
    test_spinlock_null_safe();
    test_mutex_create_destroy();
    test_mutex_lock_unlock();
    test_mutex_null_safe();
    TEST_RESULTS();
}
