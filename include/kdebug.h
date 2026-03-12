#ifndef KDEBUG_H
#define KDEBUG_H

#include "stddef.h"
#include <stdint.h>

/*
 * kdebug - Debugging, profiling, history & inspection for Pudim-Luarix
 *
 * Pudding analogies:
 *   History   = "pudding layers"     — each command stacks like layers
 *   Profiler  = "baking timer"       — timing how long the pudding bakes
 *   Inspector = "ingredient check"   — checking what's in the mix
 */

/* --- Pudding Layers: command history ring buffer --- */

#define KDEBUG_MAX_LAYERS    32u   /* max history entries (layers of pudding) */
#define KDEBUG_LAYER_SIZE   256u   /* max chars per layer */

void  kdebug_history_init(void);
void  kdebug_history_push(const char* cmd);
const char* kdebug_history_at(unsigned int index); /* 0 = most recent */
unsigned int kdebug_history_count(void);

/* --- Baking Timer: execution profiler --- */

#define KDEBUG_MAX_MARKS 16u  /* max checkpoint marks per bake session */

void     kdebug_timer_reset(void);
void     kdebug_timer_start(void);
uint64_t kdebug_timer_stop(void);              /* returns elapsed ms */
int      kdebug_timer_mark(const char* label); /* returns 1 on success */
unsigned int kdebug_timer_mark_count(void);
const char*  kdebug_timer_mark_label(unsigned int index);
uint64_t     kdebug_timer_mark_ms(unsigned int index);

/* --- Ingredient Check: heap inspector --- */

unsigned int kdebug_heap_block_count(void);
unsigned int kdebug_heap_free_block_count(void);
size_t       kdebug_heap_largest_free(void);

#endif
