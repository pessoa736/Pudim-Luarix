#ifndef KMEMORY_H
#define KMEMORY_H

#include "utypes.h"

#define KMEMORY_MAX_BLOCKS 128
#define KMEMORY_FLAG_READ  0x1u
#define KMEMORY_FLAG_WRITE 0x2u
#define KMEMORY_FLAG_EXEC  0x4u

void kmemory_init(void);
void* kmemory_allocate(uint64_t size);
int kmemory_free(void* ptr);
int kmemory_protect(void* ptr, uint32_t flags);
uint64_t kmemory_total_allocated(void);
uint32_t kmemory_block_count(void);
void kmemory_dump_stats(char* out, uint32_t out_size);

#endif
