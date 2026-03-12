#include "kmemory.h"
#include "stddef.h"

#define KMEMORY_PD_BASE_ADDR 0x202000ull
#define KMEMORY_PD_ENTRIES 512u
#define KMEMORY_PAGE_SIZE_2M 0x200000ull
#define KMEMORY_PAGE_SIZE_4K 0x1000ull
#define KMEMORY_PAGE_POOL_PAGES 256u
#define KMEMORY_PT_ENTRIES 512u
#define KMEMORY_PT_POOL_PAGES 8u
#define KMEMORY_PDE_FLAG_PRESENT 0x1ull
#define KMEMORY_PDE_FLAG_RW 0x2ull
#define KMEMORY_PDE_FLAG_PS 0x80ull
#define KMEMORY_PTE_FLAG_NX (1ull << 63)
#define KMEMORY_ADDR_MASK 0x000FFFFFFFFFF000ull
#define KMEMORY_PAGE_FAULT_PRESENT_BIT 0x1ull
#define KMEMORY_PML4_INDEX_MASK 0x1FFull
#define KMEMORY_PDPT_INDEX_MASK 0x1FFull
#define KMEMORY_PD_INDEX_MASK 0x1FFull
#define KMEMORY_NULL_GUARD_BYTES 0x1000ull

typedef struct {
    uint32_t pd_index;
    uint32_t used;
    uint64_t* table;
} kmemory_pt_slot_t;

typedef struct {
    void* ptr;
    uint64_t size;
    uint32_t flags;
    uint32_t used;
} kmemory_block_t;

static kmemory_block_t g_kmemory_blocks[KMEMORY_MAX_BLOCKS];
static uint64_t g_kmemory_total = 0;
static uint8_t g_kmemory_page_pool[KMEMORY_PAGE_POOL_PAGES][KMEMORY_PAGE_SIZE_4K] __attribute__((aligned(4096)));
static uint8_t g_kmemory_page_used[KMEMORY_PAGE_POOL_PAGES];
static uint64_t g_kmemory_pt_pool[KMEMORY_PT_POOL_PAGES][KMEMORY_PT_ENTRIES] __attribute__((aligned(4096)));
static kmemory_pt_slot_t g_kmemory_pt_slots[KMEMORY_PT_POOL_PAGES];

static uint64_t kmemory_align_up_page(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    if (size > (~0ull) - (KMEMORY_PAGE_SIZE_4K - 1ull)) {
        return 0;
    }

    return (size + (KMEMORY_PAGE_SIZE_4K - 1ull)) & ~(KMEMORY_PAGE_SIZE_4K - 1ull);
}

static int kmemory_pool_find_span(uint32_t pages_needed, uint32_t* out_start) {
    uint32_t start = 0;

    if (!out_start || pages_needed == 0 || pages_needed > KMEMORY_PAGE_POOL_PAGES) {
        return 0;
    }

    while (start + pages_needed <= KMEMORY_PAGE_POOL_PAGES) {
        uint32_t offset = 0;
        while (offset < pages_needed && !g_kmemory_page_used[start + offset]) {
            offset++;
        }

        if (offset == pages_needed) {
            *out_start = start;
            return 1;
        }

        start += offset + 1u;
    }

    return 0;
}

static inline uint64_t* kmemory_pd_base(void) {
    return (uint64_t*)(size_t)KMEMORY_PD_BASE_ADDR;
}

static inline void kmemory_invlpg(void* addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void kmemory_flush_tlb_all(void) {
    uint64_t cr3;

    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline uint64_t kmemory_read_cr3(void) {
    uint64_t value;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

static int kmemory_find_or_alloc_pt_slot(uint32_t pd_index, uint64_t** out_table) {
    uint32_t i;

    if (!out_table) {
        return 0;
    }

    for (i = 0; i < KMEMORY_PT_POOL_PAGES; i++) {
        if (g_kmemory_pt_slots[i].used && g_kmemory_pt_slots[i].pd_index == pd_index) {
            *out_table = g_kmemory_pt_slots[i].table;
            return 1;
        }
    }

    for (i = 0; i < KMEMORY_PT_POOL_PAGES; i++) {
        if (!g_kmemory_pt_slots[i].used) {
            g_kmemory_pt_slots[i].used = 1;
            g_kmemory_pt_slots[i].pd_index = pd_index;
            g_kmemory_pt_slots[i].table = g_kmemory_pt_pool[i];
            *out_table = g_kmemory_pt_slots[i].table;
            return 1;
        }
    }

    return 0;
}

static int kmemory_split_2m_pde(uint32_t pd_index) {
    uint64_t* pd;
    uint64_t pde;
    uint64_t pde_addr;
    uint64_t pte_flags;
    uint64_t* pt;
    uint32_t i;

    if (pd_index >= KMEMORY_PD_ENTRIES) {
        return 0;
    }

    pd = kmemory_pd_base();
    pde = pd[pd_index];

    if ((pde & KMEMORY_PDE_FLAG_PRESENT) == 0) {
        return 0;
    }

    if ((pde & KMEMORY_PDE_FLAG_PS) == 0) {
        return 1;
    }

    if (!kmemory_find_or_alloc_pt_slot(pd_index, &pt)) {
        return 0;
    }

    pde_addr = pde & 0x000FFFFFFFE00000ull;
    pte_flags = (pde & (~KMEMORY_PDE_FLAG_PS & 0xFFFull)) | (pde & KMEMORY_PTE_FLAG_NX);

    for (i = 0; i < KMEMORY_PT_ENTRIES; i++) {
        pt[i] = (pde_addr + ((uint64_t)i * KMEMORY_PAGE_SIZE_4K)) | pte_flags;
    }

    pd[pd_index] = ((uint64_t)(size_t)pt & KMEMORY_ADDR_MASK) | (pde & (0xFFFull & ~KMEMORY_PDE_FLAG_PS));
    kmemory_flush_tlb_all();
    return 1;
}

static int kmemory_apply_protection_4k(uint64_t addr, uint64_t size, uint32_t flags) {
    uint64_t start;
    uint64_t last;
    uint64_t cur;
    uint64_t* pd;

    if (size == 0) {
        return 0;
    }

    if (addr > (~0ull) - (size - 1ull)) {
        return 0;
    }

    start = addr;
    last = addr + size - 1ull;
    pd = kmemory_pd_base();

    cur = start;
    while (cur <= last) {
        uint32_t pd_index = (uint32_t)(cur / KMEMORY_PAGE_SIZE_2M);
        uint32_t pt_index;
        uint64_t pde;
        uint64_t* pt;
        uint64_t pte;

        if (pd_index >= KMEMORY_PD_ENTRIES) {
            return 0;
        }

        if (!kmemory_split_2m_pde(pd_index)) {
            return 0;
        }

        pde = pd[pd_index];
        if ((pde & KMEMORY_PDE_FLAG_PRESENT) == 0 || (pde & KMEMORY_PDE_FLAG_PS) != 0) {
            return 0;
        }

        pt = (uint64_t*)(size_t)(pde & KMEMORY_ADDR_MASK);
        pt_index = (uint32_t)((cur / KMEMORY_PAGE_SIZE_4K) & 0x1FFull);

        pte = pt[pt_index];
        if ((pte & KMEMORY_PDE_FLAG_PRESENT) == 0) {
            return 0;
        }

        if (flags & KMEMORY_FLAG_WRITE) {
            pte |= KMEMORY_PDE_FLAG_RW;
        } else {
            pte &= ~KMEMORY_PDE_FLAG_RW;
        }

        if (flags & KMEMORY_FLAG_EXEC) {
            pte &= ~KMEMORY_PTE_FLAG_NX;
        } else {
            pte |= KMEMORY_PTE_FLAG_NX;
        }

        pt[pt_index] = pte;
        kmemory_invlpg((void*)(size_t)(cur & ~(KMEMORY_PAGE_SIZE_4K - 1ull)));

        if (cur > (~0ull) - KMEMORY_PAGE_SIZE_4K) {
            break;
        }

        cur += KMEMORY_PAGE_SIZE_4K;
    }

    return 1;
}

static void kmemory_u64_to_str(uint64_t value, char* out, uint32_t out_size) {
    char tmp[32];
    uint32_t n = 0;
    uint32_t p = 0;

    if (!out || out_size < 2) {
        return;
    }

    if (value == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    while (value > 0 && n < (uint32_t)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (n > 0 && p + 1 < out_size) {
        out[p++] = tmp[--n];
    }

    out[p] = 0;
}

static uint32_t kmemory_strlen(const char* s) {
    uint32_t n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static void kmemory_strcat(char* out, uint32_t out_size, const char* src) {
    uint32_t p;
    uint32_t i = 0;

    if (!out || !src || out_size == 0) {
        return;
    }

    p = kmemory_strlen(out);
    while (src[i] && p + 1 < out_size) {
        out[p++] = src[i++];
    }

    out[p] = 0;
}

void kmemory_init(void) {
    uint32_t i;

    for (i = 0; i < KMEMORY_MAX_BLOCKS; i++) {
        g_kmemory_blocks[i].ptr = 0;
        g_kmemory_blocks[i].size = 0;
        g_kmemory_blocks[i].flags = 0;
        g_kmemory_blocks[i].used = 0;
    }

    for (i = 0; i < KMEMORY_PT_POOL_PAGES; i++) {
        g_kmemory_pt_slots[i].pd_index = 0;
        g_kmemory_pt_slots[i].used = 0;
        g_kmemory_pt_slots[i].table = 0;
    }

    for (i = 0; i < KMEMORY_PAGE_POOL_PAGES; i++) {
        g_kmemory_page_used[i] = 0;
    }

    g_kmemory_total = 0;
}

void* kmemory_allocate(uint64_t size) {
    uint32_t i;
    uint32_t start_page;
    uint32_t page_count;
    uint64_t alloc_size;
    void* ptr;

    if (size == 0) {
        return 0;
    }

    alloc_size = kmemory_align_up_page(size);
    if (alloc_size == 0) {
        return 0;
    }

    page_count = (uint32_t)(alloc_size / KMEMORY_PAGE_SIZE_4K);
    if (page_count == 0 || page_count > KMEMORY_PAGE_POOL_PAGES) {
        return 0;
    }

    if (g_kmemory_total > (uint64_t)(~0ull) - alloc_size) {
        return 0;
    }

    if (!kmemory_pool_find_span(page_count, &start_page)) {
        return 0;
    }

    for (i = 0; i < KMEMORY_MAX_BLOCKS; i++) {
        if (!g_kmemory_blocks[i].used) {
            uint32_t page_index;

            for (page_index = 0; page_index < page_count; page_index++) {
                g_kmemory_page_used[start_page + page_index] = 1;
            }

            ptr = (void*)&g_kmemory_page_pool[start_page][0];

            g_kmemory_blocks[i].ptr = ptr;
            g_kmemory_blocks[i].size = alloc_size;
            g_kmemory_blocks[i].flags = KMEMORY_FLAG_READ | KMEMORY_FLAG_WRITE;
            g_kmemory_blocks[i].used = 1;
            g_kmemory_total += alloc_size;

            if (!kmemory_apply_protection_4k(
                    (uint64_t)(size_t)g_kmemory_blocks[i].ptr,
                    g_kmemory_blocks[i].size,
                    g_kmemory_blocks[i].flags)) {
                uint32_t rollback_index;

                g_kmemory_total -= alloc_size;
                g_kmemory_blocks[i].ptr = 0;
                g_kmemory_blocks[i].size = 0;
                g_kmemory_blocks[i].flags = 0;
                g_kmemory_blocks[i].used = 0;
                for (rollback_index = 0; rollback_index < page_count; rollback_index++) {
                    g_kmemory_page_used[start_page + rollback_index] = 0;
                }
                return 0;
            }

            return ptr;
        }
    }

    return 0;
}

int kmemory_free(void* ptr) {
    uint32_t i;

    if (!ptr) {
        return 0;
    }

    for (i = 0; i < KMEMORY_MAX_BLOCKS; i++) {
        if (g_kmemory_blocks[i].used && g_kmemory_blocks[i].ptr == ptr) {
            uint32_t start_page;
            uint32_t page_count;
            uint32_t page_index;

            if (g_kmemory_total >= g_kmemory_blocks[i].size) {
                g_kmemory_total -= g_kmemory_blocks[i].size;
            } else {
                g_kmemory_total = 0;
            }

            start_page = (uint32_t)(((uint8_t*)ptr - &g_kmemory_page_pool[0][0]) / KMEMORY_PAGE_SIZE_4K);
            page_count = (uint32_t)(g_kmemory_blocks[i].size / KMEMORY_PAGE_SIZE_4K);

            if (page_count == 0) {
                page_count = 1;
            }

            if (start_page < KMEMORY_PAGE_POOL_PAGES) {
                for (page_index = 0; page_index < page_count && (start_page + page_index) < KMEMORY_PAGE_POOL_PAGES; page_index++) {
                    g_kmemory_page_used[start_page + page_index] = 0;
                }
            }

            g_kmemory_blocks[i].ptr = 0;
            g_kmemory_blocks[i].size = 0;
            g_kmemory_blocks[i].flags = 0;
            g_kmemory_blocks[i].used = 0;
            return 1;
        }
    }

    return 0;
}

int kmemory_protect(void* ptr, uint32_t flags) {
    uint32_t i;

    if (!ptr) {
        return 0;
    }

    flags &= (KMEMORY_FLAG_READ | KMEMORY_FLAG_WRITE | KMEMORY_FLAG_EXEC);

    for (i = 0; i < KMEMORY_MAX_BLOCKS; i++) {
        if (g_kmemory_blocks[i].used && g_kmemory_blocks[i].ptr == ptr) {
                if (!kmemory_apply_protection_4k(
                    (uint64_t)(size_t)g_kmemory_blocks[i].ptr,
                    g_kmemory_blocks[i].size,
                    flags)) {
                return 0;
            }

            g_kmemory_blocks[i].flags = flags;
            return 1;
        }
    }

    return 0;
}

int kmemory_handle_page_fault(uint64_t fault_addr, uint64_t error_code) {
    uint64_t cr3;
    uint64_t* pml4;
    uint64_t pml4e;
    uint64_t* pdpt;
    uint64_t pdpte;
    uint64_t* pd;
    uint64_t pde;
    uint64_t map_base;
    uint64_t pml4_index;
    uint64_t pdpt_index;
    uint64_t pd_index;

    if ((error_code & KMEMORY_PAGE_FAULT_PRESENT_BIT) != 0) {
        return 0;
    }

    if (fault_addr < KMEMORY_NULL_GUARD_BYTES) {
        return 0;
    }

    pml4_index = (fault_addr >> 39) & KMEMORY_PML4_INDEX_MASK;
    pdpt_index = (fault_addr >> 30) & KMEMORY_PDPT_INDEX_MASK;
    pd_index = (fault_addr >> 21) & KMEMORY_PD_INDEX_MASK;

    if (pml4_index != 0 || pdpt_index != 0) {
        return 0;
    }

    cr3 = kmemory_read_cr3();
    pml4 = (uint64_t*)(size_t)(cr3 & KMEMORY_ADDR_MASK);
    if (!pml4) {
        return 0;
    }

    pml4e = pml4[pml4_index];
    if ((pml4e & KMEMORY_PDE_FLAG_PRESENT) == 0) {
        return 0;
    }

    pdpt = (uint64_t*)(size_t)(pml4e & KMEMORY_ADDR_MASK);
    if (!pdpt) {
        return 0;
    }

    pdpte = pdpt[pdpt_index];
    if ((pdpte & KMEMORY_PDE_FLAG_PRESENT) == 0) {
        return 0;
    }

    pd = (uint64_t*)(size_t)(pdpte & KMEMORY_ADDR_MASK);
    if (!pd) {
        return 0;
    }

    pde = pd[pd_index];
    if ((pde & KMEMORY_PDE_FLAG_PRESENT) != 0) {
        return 0;
    }

    map_base = fault_addr & ~(KMEMORY_PAGE_SIZE_2M - 1ull);
    pd[pd_index] = map_base | KMEMORY_PDE_FLAG_PRESENT | KMEMORY_PDE_FLAG_RW | KMEMORY_PDE_FLAG_PS;
    kmemory_invlpg((void*)(size_t)(fault_addr & ~(KMEMORY_PAGE_SIZE_4K - 1ull)));
    return 1;
}

uint64_t kmemory_total_allocated(void) {
    return g_kmemory_total;
}

uint32_t kmemory_block_count(void) {
    uint32_t i;
    uint32_t total = 0;

    for (i = 0; i < KMEMORY_MAX_BLOCKS; i++) {
        if (g_kmemory_blocks[i].used) {
            total++;
        }
    }

    return total;
}

void kmemory_dump_stats(char* out, uint32_t out_size) {
    char numbuf[32];

    if (!out || out_size == 0) {
        return;
    }

    out[0] = 0;
    kmemory_strcat(out, out_size, "allocated=");
    kmemory_u64_to_str(kmemory_total_allocated(), numbuf, sizeof(numbuf));
    kmemory_strcat(out, out_size, numbuf);
    kmemory_strcat(out, out_size, " bytes, blocks=");
    kmemory_u64_to_str((uint64_t)kmemory_block_count(), numbuf, sizeof(numbuf));
    kmemory_strcat(out, out_size, numbuf);
    kmemory_strcat(out, out_size, ", mmu_rwx=4k");
}
