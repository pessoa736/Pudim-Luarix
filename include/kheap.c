#include "kheap.h"
#include "utypes.h"

extern uint8_t __kernel_end;

/*
 Endereço inicial da heap.

 Em kernels simples esse valor costuma vir do linker script
 ou ser definido manualmente.
 */
#define HEAP_SIZE  0x100000

#define HEAP_ALIGNMENT 8

#define BLOCK_MAGIC_FREE  0xFEE1FEE1
#define BLOCK_MAGIC_ALLOC 0xA110CA11

/*
 Header que fica antes de cada bloco de memória.

 Layout real na RAM:

 [header][memoria_util]
 */
typedef struct block_header {

    size_t size;                // tamanho da área utilizável
    int free;                   // 1 = bloco livre, 0 = ocupado
    uint32_t magic;             // validação simples de integridade

    struct block_header* next;  // próximo bloco na heap
    struct block_header* prev;  // bloco anterior

} block_header_t;

/*
 Ponteiro para o primeiro bloco da heap.
 */
static block_header_t* heap_start = NULL;
static size_t heap_region_start = 0;
static size_t heap_region_end = 0;
static volatile uint32_t g_heap_lock = 0;

static inline void heap_lock(void) {
    while (1) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        uint32_t oldval;

        asm volatile(
            "lock cmpxchgl %2, %1"
            : "=a"(oldval), "+m"(g_heap_lock)
            : "r"(desired), "0"(expected)
            : "cc"
        );

        if (oldval == expected) {
            return;
        }

        asm volatile("pause");
    }
}

static inline void heap_unlock(void) {
    g_heap_lock = 0;
}


static size_t align_up(size_t value) {

    return (value + (HEAP_ALIGNMENT - 1)) & ~(HEAP_ALIGNMENT - 1);
}


static int ptr_in_heap(void* ptr) {

    size_t addr = (size_t)ptr;

    return (addr >= heap_region_start) && (addr < heap_region_end);
}

static int block_header_valid(block_header_t* block) {
    size_t block_addr;
    size_t payload_end;

    if (!block) {
        return 0;
    }

    if (!ptr_in_heap(block)) {
        return 0;
    }

    if (block->magic != BLOCK_MAGIC_FREE && block->magic != BLOCK_MAGIC_ALLOC) {
        return 0;
    }

    block_addr = (size_t)block;
    if (block->size > HEAP_SIZE) {
        return 0;
    }

    if (block_addr > heap_region_end - sizeof(block_header_t)) {
        return 0;
    }

    if (block->size > (heap_region_end - block_addr - sizeof(block_header_t))) {
        return 0;
    }

    payload_end = block_addr + sizeof(block_header_t) + block->size;
    if (payload_end > heap_region_end) {
        return 0;
    }

    if (block->next && !ptr_in_heap(block->next)) {
        return 0;
    }

    if (block->prev && !ptr_in_heap(block->prev)) {
        return 0;
    }

    return 1;
}


/*
 Inicializa a heap do kernel.

 Cria um único bloco livre ocupando toda a heap.
 */
void heap_init() {

    heap_region_start = align_up((size_t)&__kernel_end);
    heap_region_end = heap_region_start + HEAP_SIZE;

    heap_start = (block_header_t*)heap_region_start;

    heap_start->size = (HEAP_SIZE - sizeof(block_header_t)) & ~(HEAP_ALIGNMENT - 1);
    heap_start->free = 1;
    heap_start->magic = BLOCK_MAGIC_FREE;

    heap_start->next = NULL;
    heap_start->prev = NULL;
}


/*
 Divide um bloco grande em dois.

 Exemplo:

 [256 livre] -> pedido 64

 resultado:

 [64 usado][192 livre]
 */
static void split_block(block_header_t* block, size_t size) {

    block_header_t* new_block;

    if (!block_header_valid(block)) {
        return;
    }

    if (size > block->size || block->size - size < sizeof(block_header_t)) {
        return;
    }

    new_block = (block_header_t*)
        ((char*)block + sizeof(block_header_t) + size);

    if (!ptr_in_heap(new_block)) {
        return;
    }

    new_block->size = block->size - size - sizeof(block_header_t);
    new_block->free = 1;
    new_block->magic = BLOCK_MAGIC_FREE;

    new_block->next = block->next;
    new_block->prev = block;

    if(block->next)
        block->next->prev = new_block;

    block->next = new_block;
    block->size = size;
}


/*
 Procura o primeiro bloco livre grande o suficiente.
 Algoritmo: First Fit.
 */
static block_header_t* find_free_block(size_t size) {

    block_header_t* current = heap_start;

    while(current) {

        if (!block_header_valid(current))
            return NULL;

        if(current->free && current->size >= size)
            return current;

        current = current->next;
    }

    return NULL;
}


/*
 Retorna o header associado a um ponteiro retornado por kmalloc.
 */
static block_header_t* get_block_header(void* ptr) {

    return (block_header_t*)ptr - 1;
}


/*
 Junta dois blocos livres consecutivos.
 */
static void merge_blocks(block_header_t* block) {

    if (!block_header_valid(block)) {
        return;
    }

    if(block->next && block->next->free) {

        if (!block_header_valid(block->next)) {
            block->next = NULL;
            return;
        }

        block->size += sizeof(block_header_t)
                     + block->next->size;

        block->next = block->next->next;

        if(block->next)
            block->next->prev = block;
    }
}


/*
 Aloca memória da heap do kernel.
 */
void* kmalloc(size_t size) {

    block_header_t* block;
    void* result;

    if(size == 0)
        return NULL;

    if(!heap_start)
        return NULL;

    heap_lock();

    size = align_up(size);

    block = find_free_block(size);

    if(!block) {
        heap_unlock();
        return NULL;
    }

    /*
     Divide o bloco se ele for maior que o necessário.
     */
    if(block->size >= size + sizeof(block_header_t) + HEAP_ALIGNMENT)
        split_block(block, size);

    block->free = 0;
    block->magic = BLOCK_MAGIC_ALLOC;

    /*
     Retorna o endereço logo após o header.
     */
    result = (void*)(block + 1);
    heap_unlock();
    return result;
}


void* krealloc(void* ptr, size_t size) {
    block_header_t* block;
    void* new_ptr;
    size_t copy_size;
    size_t i;
    uint8_t* src;
    uint8_t* dst;

    if(!ptr)
        return kmalloc(size);

    if(size == 0) {
        kfree(ptr);
        return NULL;
    }

    if(!ptr_in_heap(ptr))
        return NULL;

    block = get_block_header(ptr);

    if(!ptr_in_heap(block))
        return NULL;

    if(block->magic != BLOCK_MAGIC_ALLOC)
        return NULL;

    size = align_up(size);

    if(block->size >= size)
        return ptr;

    new_ptr = kmalloc(size);

    if(!new_ptr)
        return NULL;

    copy_size = block->size;
    src = (uint8_t*)ptr;
    dst = (uint8_t*)new_ptr;

    for(i = 0; i < copy_size; i++)
        dst[i] = src[i];

    kfree(ptr);

    return new_ptr;
}


/*
 Libera memória previamente alocada.
 */
void kfree(void* ptr) {

    block_header_t* block;

    if(!ptr)
        return;

    if(!ptr_in_heap(ptr))
        return;

    block = get_block_header(ptr);

    if(!ptr_in_heap(block))
        return;

    if(!block_header_valid(block))
        return;

    if(block->magic != BLOCK_MAGIC_ALLOC)
        return;

    heap_lock();

    block->free = 1;
    block->magic = BLOCK_MAGIC_FREE;

    /*
     Tenta fundir com o próximo bloco.
     */
    merge_blocks(block);

    /*
     Tenta fundir com o bloco anterior.
     */
    if(block->prev && block->prev->free)
        merge_blocks(block->prev);

    heap_unlock();
}
size_t kheap_free_bytes(void) {
    if (!heap_start) {
        return 0;
    }

    size_t free_bytes = 0;
    block_header_t* current = heap_start;

    while (current) {
        if (!block_header_valid(current)) {
            break;
        }
        if (current->free) {
            free_bytes += current->size;
        }
        current = current->next;
    }

    return free_bytes;
}

size_t kheap_total_bytes(void) {
    if (!heap_start) {
        return 0;
    }

    size_t total_bytes = 0;
    block_header_t* current = heap_start;

    while (current) {
        if (!block_header_valid(current)) {
            break;
        }
        total_bytes += current->size;
        current = current->next;
    }

    return total_bytes;
}

unsigned int kheap_block_count(void) {
    unsigned int count = 0;
    block_header_t* current = heap_start;

    while (current) {
        if (!block_header_valid(current)) {
            break;
        }
        count++;
        current = current->next;
    }

    return count;
}

unsigned int kheap_free_block_count(void) {
    unsigned int count = 0;
    block_header_t* current = heap_start;

    while (current) {
        if (!block_header_valid(current)) {
            break;
        }
        if (current->free) {
            count++;
        }
        current = current->next;
    }

    return count;
}

size_t kheap_largest_free_block(void) {
    size_t largest = 0;
    block_header_t* current = heap_start;

    while (current) {
        if (!block_header_valid(current)) {
            break;
        }
        if (current->free && current->size > largest) {
            largest = current->size;
        }
        current = current->next;
    }

    return largest;
}
