#ifndef KHEAP_H
#define KHEAP_H

#include "stddef.h"


/*
 Interface pública do allocator da heap do kernel.
*/

void heap_init(void);

/*
 Aloca memória da heap do kernel.
 Retorna NULL se não houver espaço.
*/
void* kmalloc(size_t size);

/*
 Redimensiona memória alocada por kmalloc.
 Retorna NULL em falha (ponteiro original permanece válido).
*/
void* krealloc(void* ptr, size_t size);

/*
 Libera memória previamente alocada por kmalloc.
*/
void kfree(void* ptr);

/*
 Returns number of free bytes available in heap.
*/
size_t kheap_free_bytes(void);

/*
Returns total usable bytes managed by the heap.
*/
size_t kheap_total_bytes(void);

/*
Returns total number of blocks (free + allocated) in the heap.
*/
unsigned int kheap_block_count(void);

/*
Returns number of free blocks in the heap.
*/
unsigned int kheap_free_block_count(void);

/*
Returns the size of the largest free block in the heap.
*/
size_t kheap_largest_free_block(void);

#endif
