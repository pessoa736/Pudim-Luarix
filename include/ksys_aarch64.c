/*
 * ksys_aarch64.c — System info for ARM64 (Bundt Pan Edition)
 *
 * Same pudding recipe as x86_64, but baked in a bundt pan
 * instead of a rectangle. CPU info comes from MIDR_EL1 and
 * friends instead of CPUID; memory map is provided by the
 * bootloader or device tree (stubbed for now).
 */
#include "ksys.h"
#include "kheap.h"

#define KSYS_CPU_VENDOR_LEN 13u
#define KSYS_CPU_BRAND_LEN  49u

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

/*
 * ARM CPU vendor detection via MIDR_EL1.
 * Bits [31:24] = Implementer code.
 */
static void ksys_detect_cpu_vendor(char* out, uint32_t out_size) {
    uint64_t midr;
    uint32_t implementer;

    asm volatile("mrs %0, midr_el1" : "=r"(midr));
    implementer = (uint32_t)((midr >> 24) & 0xFFu);

    switch (implementer) {
        case 0x41: ksys_copy_string(out, out_size, "ARM");        break;
        case 0x42: ksys_copy_string(out, out_size, "Broadcom");   break;
        case 0x43: ksys_copy_string(out, out_size, "Cavium");     break;
        case 0x44: ksys_copy_string(out, out_size, "DEC");        break;
        case 0x46: ksys_copy_string(out, out_size, "Fujitsu");    break;
        case 0x48: ksys_copy_string(out, out_size, "HiSilicon");  break;
        case 0x49: ksys_copy_string(out, out_size, "Infineon");   break;
        case 0x4D: ksys_copy_string(out, out_size, "Motorola");   break;
        case 0x4E: ksys_copy_string(out, out_size, "NVIDIA");     break;
        case 0x50: ksys_copy_string(out, out_size, "APM");        break;
        case 0x51: ksys_copy_string(out, out_size, "Qualcomm");   break;
        case 0x53: ksys_copy_string(out, out_size, "Samsung");    break;
        case 0x56: ksys_copy_string(out, out_size, "Marvell");    break;
        case 0x61: ksys_copy_string(out, out_size, "Apple");      break;
        case 0x66: ksys_copy_string(out, out_size, "Faraday");    break;
        case 0x69: ksys_copy_string(out, out_size, "Intel");      break;
        case 0xC0: ksys_copy_string(out, out_size, "Ampere");     break;
        default:   ksys_copy_string(out, out_size, "unknown");    break;
    }
}

/*
 * ARM CPU brand detection via MIDR_EL1.
 * Bits [15:4] = PartNum. Combined with implementer for brand string.
 */
static void ksys_detect_cpu_brand(char* out, uint32_t out_size) {
    uint64_t midr;
    uint32_t implementer;
    uint32_t partnum;

    asm volatile("mrs %0, midr_el1" : "=r"(midr));
    implementer = (uint32_t)((midr >> 24) & 0xFFu);
    partnum = (uint32_t)((midr >> 4) & 0xFFFu);

    /* ARM Ltd cores */
    if (implementer == 0x41) {
        switch (partnum) {
            case 0xD03: ksys_copy_string(out, out_size, "ARM Cortex-A53");  return;
            case 0xD04: ksys_copy_string(out, out_size, "ARM Cortex-A35");  return;
            case 0xD05: ksys_copy_string(out, out_size, "ARM Cortex-A55");  return;
            case 0xD07: ksys_copy_string(out, out_size, "ARM Cortex-A57");  return;
            case 0xD08: ksys_copy_string(out, out_size, "ARM Cortex-A72");  return;
            case 0xD09: ksys_copy_string(out, out_size, "ARM Cortex-A73");  return;
            case 0xD0A: ksys_copy_string(out, out_size, "ARM Cortex-A75");  return;
            case 0xD0B: ksys_copy_string(out, out_size, "ARM Cortex-A76");  return;
            case 0xD0D: ksys_copy_string(out, out_size, "ARM Cortex-A77");  return;
            case 0xD40: ksys_copy_string(out, out_size, "ARM Neoverse-V1"); return;
            case 0xD41: ksys_copy_string(out, out_size, "ARM Cortex-A78");  return;
            case 0xD44: ksys_copy_string(out, out_size, "ARM Cortex-X1");   return;
            case 0xD46: ksys_copy_string(out, out_size, "ARM Cortex-A510"); return;
            case 0xD47: ksys_copy_string(out, out_size, "ARM Cortex-A710"); return;
            case 0xD48: ksys_copy_string(out, out_size, "ARM Cortex-X2");   return;
            case 0xD49: ksys_copy_string(out, out_size, "ARM Neoverse-N2"); return;
            default: break;
        }
    }

    ksys_copy_string(out, out_size, "AArch64 CPU");
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
    return "Pudim-Luarix aarch64 v0.1.0";
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

/* No firmware memory map on aarch64 (no E820) — stubbed */
uint32_t ksys_memory_map_count(void) {
    return 0;
}

uint64_t ksys_memory_map_base(uint32_t index) {
    (void)index;
    return 0;
}

uint64_t ksys_memory_map_length(uint32_t index) {
    (void)index;
    return 0;
}

uint32_t ksys_memory_map_type(uint32_t index) {
    (void)index;
    return 0;
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
    /* Default 128MB for QEMU virt — can be overridden by DTB parsing later */
    g_sysinfo.memory_total = 128ull * 1024ull * 1024ull;
    g_sysinfo.boot_media_total = 0;
    g_sysinfo.kernel_image_size = 0;
    g_sysinfo.cpu_count = 1;
    g_sysinfo.cpu_physical_cores = 1;
    ksys_detect_cpu_vendor(g_sysinfo.cpu_vendor, KSYS_CPU_VENDOR_LEN);
    ksys_detect_cpu_brand(g_sysinfo.cpu_brand, KSYS_CPU_BRAND_LEN);
}

void ksys_tick(void) {
    /* Assume 100Hz timer (10ms per tick) */
    g_sysinfo.uptime_ms += 10;
    g_sysinfo.uptime_us += 10000;
}
