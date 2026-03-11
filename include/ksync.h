#ifndef KSYNC_H
#define KSYNC_H

#include <stdint.h>

/*
 Synchronization primitives for multi-process safety.
 
 Simplistic implementations:
 - Spinlock: busy-wait (efficient on bare metal, minimal latency)
 - Mutex: spinlock + counter (simple mutual exclusion)
*/

typedef struct {
    volatile uint32_t locked;  /* 0 = unlocked, 1 = locked */
} ksync_spinlock_t;

typedef struct {
    volatile uint32_t owner_pid;  /* PID of current lock owner (0 = free) */
    volatile uint32_t count;      /* Lock count (reentrant) */
} ksync_mutex_t;

/* Spinlock operations */
ksync_spinlock_t* ksync_spinlock_create(void);
void ksync_spinlock_lock(ksync_spinlock_t* lock);
void ksync_spinlock_unlock(ksync_spinlock_t* lock);
void ksync_spinlock_destroy(ksync_spinlock_t* lock);

/* Mutex operations */
ksync_mutex_t* ksync_mutex_create(void);
void ksync_mutex_lock(ksync_mutex_t* lock);
void ksync_mutex_unlock(ksync_mutex_t* lock);
void ksync_mutex_destroy(ksync_mutex_t* lock);

#endif
