#include "ksync.h"
#include "kheap.h"

/* Spinlock: busy-wait atomically */
static void spin_lock_acquire(volatile uint32_t* lock) {
    while (1) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        
        /* Try atomic compare-and-swap */
        uint32_t oldval;
        asm volatile (
            "lock cmpxchgl %2, %1"
            : "=a" (oldval), "+m" (*lock)
            : "r" (desired), "0" (expected)
            : "cc"
        );
        
        if (oldval == expected) {
            /* Successfully acquired lock */
            return;
        }
        
        /* Failed, spin and retry */
        asm volatile ("pause");
    }
}

static void spin_lock_release(volatile uint32_t* lock) {
    *lock = 0;
}

/* Spinlock API */
ksync_spinlock_t* ksync_spinlock_create(void) {
    ksync_spinlock_t* lock = (ksync_spinlock_t*)kmalloc(sizeof(ksync_spinlock_t));
    if (!lock) {
        return NULL;
    }
    lock->locked = 0;
    return lock;
}

void ksync_spinlock_lock(ksync_spinlock_t* lock) {
    if (!lock) return;
    spin_lock_acquire(&lock->locked);
}

void ksync_spinlock_unlock(ksync_spinlock_t* lock) {
    if (!lock) return;
    spin_lock_release(&lock->locked);
}

void ksync_spinlock_destroy(ksync_spinlock_t* lock) {
    if (lock) {
        kfree(lock);
    }
}

/* Mutex API (non-reentrant for now) */
ksync_mutex_t* ksync_mutex_create(void) {
    ksync_mutex_t* lock = (ksync_mutex_t*)kmalloc(sizeof(ksync_mutex_t));
    if (!lock) {
        return NULL;
    }
    lock->owner_pid = 0;
    lock->count = 0;
    return lock;
}

void ksync_mutex_lock(ksync_mutex_t* lock) {
    if (!lock) return;
    
    /* Simple spinlock for mutex */
    while (1) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        
        uint32_t oldval;
        asm volatile (
            "lock cmpxchgl %2, %1"
            : "=a" (oldval), "+m" (lock->owner_pid)
            : "r" (desired), "0" (expected)
            : "cc"
        );
        
        if (oldval == expected) {
            lock->count = 1;
            return;
        }
        
        asm volatile ("pause");
    }
}

void ksync_mutex_unlock(ksync_mutex_t* lock) {
    if (!lock) return;
    lock->count = 0;
    lock->owner_pid = 0;
}

void ksync_mutex_destroy(ksync_mutex_t* lock) {
    if (lock) {
        kfree(lock);
    }
}
