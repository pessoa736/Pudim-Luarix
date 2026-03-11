#include "ksys.h"
#include "kheap.h"

#define KSYS_BOOT_INFO_ADDR 0x5000ull
#define KSYS_BOOT_INFO_MAGIC 0x584C4450u
#define KSYS_BOOT_INFO_E820_ADDR 0x5300ull
#define KSYS_BOOT_INFO_E820_ENTRY_SIZE 24u
#define KSYS_CPU_VENDOR_LEN 13u
#define KSYS_CPU_BRAND_LEN 49u

typedef struct {
    uint32_t magic;
    uint32_t ram_usable_low;
    uint32_t ram_usable_high;
    uint32_t boot_sectors_low;
    uint32_t boot_sectors_high;
    uint16_t sector_size;
    uint16_t e820_count;
    uint16_t e820_entry_size;
    uint16_t e820_max_entries;
    uint16_t reserved;
    uint32_t kernel_image_size_low;
    uint32_t kernel_image_size_high;
} ksys_boot_info_t;

typedef struct {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
    uint32_t attributes;
} ksys_e820_entry_t;

static volatile const ksys_boot_info_t* const g_boot_info =
    (volatile const ksys_boot_info_t*)(size_t)KSYS_BOOT_INFO_ADDR;
static volatile const ksys_e820_entry_t* const g_boot_e820_entries =
    (volatile const ksys_e820_entry_t*)(size_t)KSYS_BOOT_INFO_E820_ADDR;

static int ksys_boot_info_valid(void) {
    return g_boot_info->magic == KSYS_BOOT_INFO_MAGIC;
}

static uint64_t ksys_make_u64(uint32_t low, uint32_t high) {
    return ((uint64_t)high << 32) | (uint64_t)low;
}

static void ksys_copy_string(char* dst, uint32_t dst_size, const char* src) {
    uint32_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static void ksys_trim_trailing_spaces(char* s) {
    uint32_t end = 0;

    if (!s) {
        return;
    }

    while (s[end]) {
        end++;
    }

    while (end > 0 && s[end - 1] == ' ') {
        s[end - 1] = 0;
        end--;
    }
}

static void ksys_u64_to_hex(uint64_t value, char* out, uint32_t out_size) {
    static const char hex_digits[] = "0123456789ABCDEF";
    uint32_t shift;
    uint32_t pos = 0;
    int started = 0;

    if (!out || out_size < 4) {
        return;
    }

    out[pos++] = '0';
    out[pos++] = 'x';

    for (shift = 60; shift > 0; shift -= 4) {
        uint32_t nibble = (uint32_t)((value >> shift) & 0xFull);

        if (!started && nibble == 0) {
            continue;
        }

        if (pos + 1 >= out_size) {
            break;
        }

        out[pos++] = hex_digits[nibble];
        started = 1;
    }

    if (pos + 1 < out_size) {
        out[pos++] = hex_digits[(uint32_t)(value & 0xFull)];
    }

    out[pos] = 0;
}

static void ksys_detect_cpu_vendor(char* out, uint32_t out_size) {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    char vendor[KSYS_CPU_VENDOR_LEN];

    asm volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0), "c"(0)
    );

    ((uint32_t*)vendor)[0] = ebx;
    ((uint32_t*)vendor)[1] = edx;
    ((uint32_t*)vendor)[2] = ecx;
    vendor[12] = 0;
    ksys_copy_string(out, out_size, vendor);
}

static void ksys_detect_cpu_brand(char* out, uint32_t out_size) {
    uint32_t max_extended;
    uint32_t regs[4];
    char brand[KSYS_CPU_BRAND_LEN];
    uint32_t i;

    asm volatile(
        "cpuid"
        : "=a"(max_extended), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
        : "a"(0x80000000u), "c"(0)
    );

    if (max_extended < 0x80000004u) {
        ksys_copy_string(out, out_size, "unknown");
        return;
    }

    for (i = 0; i < 3; i++) {
        asm volatile(
            "cpuid"
            : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
            : "a"(0x80000002u + i), "c"(0)
        );

        ((uint32_t*)brand)[i * 4 + 0] = regs[0];
        ((uint32_t*)brand)[i * 4 + 1] = regs[1];
        ((uint32_t*)brand)[i * 4 + 2] = regs[2];
        ((uint32_t*)brand)[i * 4 + 3] = regs[3];
    }

    brand[48] = 0;
    ksys_trim_trailing_spaces(brand);
    ksys_copy_string(out, out_size, brand);
}

static uint32_t ksys_detect_cpu_logical_count(void) {
    uint32_t max_basic = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

    asm volatile(
        "cpuid"
        : "=a"(max_basic), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0), "c"(0)
    );

    if (max_basic < 1) {
        return 1;
    }

    asm volatile(
        "cpuid"
        : "=a"(max_basic), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1), "c"(0)
    );

    ebx = (ebx >> 16) & 0xFFu;
    return ebx ? ebx : 1u;
}

static uint32_t ksys_detect_cpu_count(void) {
    uint32_t logical = ksys_detect_cpu_logical_count();
    uint32_t max_basic = 0;
    uint32_t max_extended = 0;
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

    asm volatile(
        "cpuid"
        : "=a"(max_basic), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0), "c"(0)
    );

    asm volatile(
        "cpuid"
        : "=a"(max_extended), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x80000000u), "c"(0)
    );

    if (max_basic >= 4) {
        asm volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(4), "c"(0)
        );

        eax = ((eax >> 26) & 0x3Fu) + 1u;
        if (eax != 0) {
            return eax;
        }
    }

    if (max_extended >= 0x80000008u) {
        asm volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0x80000008u), "c"(0)
        );

        ecx = (ecx & 0xFFu) + 1u;
        if (ecx != 0) {
            return ecx;
        }
    }

    return logical;
}

static uint32_t ksys_clamp_memory_map_count(void) {
    uint32_t count;
    uint32_t max_entries;

    if (!ksys_boot_info_valid()) {
        return 0;
    }

    if (g_boot_info->e820_entry_size != KSYS_BOOT_INFO_E820_ENTRY_SIZE) {
        return 0;
    }

    count = (uint32_t)g_boot_info->e820_count;
    max_entries = (uint32_t)g_boot_info->e820_max_entries;
    if (count > max_entries) {
        count = max_entries;
    }

    return count;
}

static const volatile ksys_e820_entry_t* ksys_get_memory_map_entry(uint32_t index) {
    if (index >= ksys_clamp_memory_map_count()) {
        return 0;
    }

    return &g_boot_e820_entries[index];
}

static uint64_t ksys_detect_memory_total(void) {
    uint64_t total;

    if (!ksys_boot_info_valid()) {
        return 64ull * 1024ull * 1024ull;
    }

    total = ksys_make_u64(g_boot_info->ram_usable_low, g_boot_info->ram_usable_high);
    if (total == 0) {
        return 64ull * 1024ull * 1024ull;
    }

    return total;
}

static uint64_t ksys_detect_boot_media_total(void) {
    uint64_t sectors;
    uint64_t sector_size;

    if (!ksys_boot_info_valid()) {
        return 0;
    }

    sectors = ksys_make_u64(g_boot_info->boot_sectors_low, g_boot_info->boot_sectors_high);
    sector_size = (uint64_t)g_boot_info->sector_size;

    if (sectors == 0 || sector_size == 0) {
        return 0;
    }

    if (sectors > (~0ull) / sector_size) {
        return 0;
    }

    return sectors * sector_size;
}

static uint64_t ksys_detect_kernel_image_size(void) {
    if (!ksys_boot_info_valid()) {
        return 0;
    }

    return ksys_make_u64(g_boot_info->kernel_image_size_low, g_boot_info->kernel_image_size_high);
}

/* Global system state */
static struct {
    uint64_t uptime_ms;
    uint64_t uptime_us;
    uint64_t memory_total;
    uint64_t boot_media_total;
    uint64_t kernel_image_size;
    uint32_t cpu_count;
    uint32_t cpu_physical_cores;
    char cpu_vendor[KSYS_CPU_VENDOR_LEN];
    char cpu_brand[KSYS_CPU_BRAND_LEN];
} g_sysinfo = {0};

static char g_ksys_hex_buffer[2][19];
static uint32_t g_ksys_hex_buffer_index = 0;

static char* ksys_next_hex_buffer(void) {
    char* out = g_ksys_hex_buffer[g_ksys_hex_buffer_index & 1u];
    g_ksys_hex_buffer_index++;
    out[0] = 0;
    return out;
}

const char* ksys_version(void) {
    return "Pudim-Luarix x86_64 v0.1.0";
}

uint64_t ksys_uptime_ms(void) {
    return g_sysinfo.uptime_ms;
}

uint64_t ksys_uptime_us(void) {
    return g_sysinfo.uptime_us;
}

uint64_t ksys_memory_total(void) {
    return g_sysinfo.memory_total;
}

uint64_t ksys_memory_free(void) {
    return kheap_free_bytes();
}

uint64_t ksys_memory_used(void) {
    uint64_t total = ksys_heap_total();
    uint64_t free = ksys_memory_free();

    return total > free ? total - free : 0;
}

uint64_t ksys_heap_total(void) {
    return kheap_total_bytes();
}

uint64_t ksys_boot_media_total(void) {
    return g_sysinfo.boot_media_total;
}

uint64_t ksys_rom_total(void) {
    return g_sysinfo.boot_media_total;
}

uint64_t ksys_kernel_image_size(void) {
    return g_sysinfo.kernel_image_size;
}

uint32_t ksys_cpu_count(void) {
    return g_sysinfo.cpu_count;
}

uint32_t ksys_cpu_physical_cores(void) {
    return g_sysinfo.cpu_physical_cores;
}

const char* ksys_cpu_vendor(void) {
    return g_sysinfo.cpu_vendor;
}

const char* ksys_cpu_brand(void) {
    return g_sysinfo.cpu_brand;
}

uint32_t ksys_memory_map_count(void) {
    return ksys_clamp_memory_map_count();
}

uint64_t ksys_memory_map_base(uint32_t index) {
    const volatile ksys_e820_entry_t* entry = ksys_get_memory_map_entry(index);

    if (!entry) {
        return 0;
    }

    return ksys_make_u64(entry->base_low, entry->base_high);
}

uint64_t ksys_memory_map_length(uint32_t index) {
    const volatile ksys_e820_entry_t* entry = ksys_get_memory_map_entry(index);

    if (!entry) {
        return 0;
    }

    return ksys_make_u64(entry->length_low, entry->length_high);
}

uint32_t ksys_memory_map_type(uint32_t index) {
    const volatile ksys_e820_entry_t* entry = ksys_get_memory_map_entry(index);

    if (!entry) {
        return 0;
    }

    return entry->type;
}

const char* ksys_memory_map_base_hex(uint32_t index) {
    char* out = ksys_next_hex_buffer();

    ksys_u64_to_hex(ksys_memory_map_base(index), out, 19);
    return out;
}

const char* ksys_memory_map_length_hex(uint32_t index) {
    char* out = ksys_next_hex_buffer();

    ksys_u64_to_hex(ksys_memory_map_length(index), out, 19);
    return out;
}

void ksys_init(void) {
    g_sysinfo.uptime_ms = 0;
    g_sysinfo.uptime_us = 0;
    g_sysinfo.memory_total = ksys_detect_memory_total();
    g_sysinfo.boot_media_total = ksys_detect_boot_media_total();
    g_sysinfo.kernel_image_size = ksys_detect_kernel_image_size();
    g_sysinfo.cpu_count = ksys_detect_cpu_logical_count();
    g_sysinfo.cpu_physical_cores = ksys_detect_cpu_count();
    ksys_detect_cpu_vendor(g_sysinfo.cpu_vendor, KSYS_CPU_VENDOR_LEN);
    ksys_detect_cpu_brand(g_sysinfo.cpu_brand, KSYS_CPU_BRAND_LEN);
}

void ksys_tick(void) {
    /* Assume 100Hz timer (10ms per tick) */
    g_sysinfo.uptime_ms += 10;
    g_sysinfo.uptime_us += 10000;
}
