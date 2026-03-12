/* Unit tests for ksys.c (utility functions and getters) */
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/*
 * ksys.c has cpuid-dependent init functions that work on host x86_64.
 * It also reads boot info from fixed memory addresses (0x5000),
 * which will read garbage on the host, but boot_info_valid() will
 * return false (no magic match) so those paths safely return defaults.
 *
 * We need kheap for kheap_free_bytes/kheap_total_bytes.
 */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* Include ksys source */
#include "../include/ksys.c"

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

    /* Pre-populate sysinfo for testing getters */
    g_sysinfo.uptime_ms = 0;
    g_sysinfo.uptime_us = 0;
    g_sysinfo.memory_total = 128ULL * 1024 * 1024;
    g_sysinfo.boot_media_total = 4ULL * 1024 * 1024;
    g_sysinfo.kernel_image_size = 65536;
    g_sysinfo.cpu_count = 2;
    g_sysinfo.cpu_physical_cores = 2;
    ksys_copy_string(g_sysinfo.cpu_vendor, KSYS_CPU_VENDOR_LEN, "TestCPU");
    ksys_copy_string(g_sysinfo.cpu_brand, KSYS_CPU_BRAND_LEN, "TestBrand");
}

static void test_version(void) {
    TEST_SUITE("ksys: version");

    ASSERT_NOT_NULL(ksys_version(), "version not null");
    ASSERT(strlen(ksys_version()) > 0, "version not empty");
}

static void test_uptime(void) {
    TEST_SUITE("ksys: uptime and tick");
    setup();

    ASSERT_EQ(ksys_uptime_ms(), 0, "initial uptime_ms is 0");
    ksys_tick();
    ASSERT_EQ(ksys_uptime_ms(), 10, "10ms after 1 tick");
    ksys_tick();
    ASSERT_EQ(ksys_uptime_ms(), 20, "20ms after 2 ticks");
    ASSERT_EQ(ksys_uptime_us(), 20000, "20000us after 2 ticks");
}

static void test_memory_info(void) {
    TEST_SUITE("ksys: memory info");
    setup();

    ASSERT_EQ(ksys_memory_total(), 128ULL * 1024 * 1024, "ram total");
    ASSERT(ksys_memory_free() > 0, "heap has free bytes");
    ASSERT(ksys_heap_total() > 0, "heap has total bytes");
    ASSERT(ksys_memory_used() <= ksys_heap_total(), "used <= total");
}

static void test_boot_info(void) {
    TEST_SUITE("ksys: boot media info");
    setup();

    ASSERT_EQ(ksys_boot_media_total(), 4ULL * 1024 * 1024, "boot media");
    ASSERT_EQ(ksys_rom_total(), 4ULL * 1024 * 1024, "rom same as boot media");
    ASSERT_EQ(ksys_kernel_image_size(), 65536, "kernel image size");
}

static void test_cpu_info(void) {
    TEST_SUITE("ksys: CPU info");
    setup();

    ASSERT_EQ(ksys_cpu_count(), 2, "cpu count");
    ASSERT_EQ(ksys_cpu_physical_cores(), 2, "physical cores");
    ASSERT_STR_EQ(ksys_cpu_vendor(), "TestCPU", "vendor");
    ASSERT_STR_EQ(ksys_cpu_brand(), "TestBrand", "brand");
}

static void test_u64_to_hex(void) {
    char buf[20];

    TEST_SUITE("ksys: u64_to_hex");

    ksys_u64_to_hex(0, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "0x0", "hex 0");

    ksys_u64_to_hex(255, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "0xFF", "hex 255");

    ksys_u64_to_hex(0x1000, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "0x1000", "hex 0x1000");
}

static void test_copy_string(void) {
    char buf[16];

    TEST_SUITE("ksys: copy_string");

    ksys_copy_string(buf, sizeof(buf), "hello");
    ASSERT_STR_EQ(buf, "hello", "copies string");

    ksys_copy_string(buf, 4, "toolong");
    ASSERT_STR_EQ(buf, "too", "truncates");

    ksys_copy_string(buf, sizeof(buf), NULL);
    ASSERT_STR_EQ(buf, "", "null src -> empty");
}

static void test_trim_trailing_spaces(void) {
    char buf[16];

    TEST_SUITE("ksys: trim_trailing_spaces");

    strcpy(buf, "test   ");
    ksys_trim_trailing_spaces(buf);
    ASSERT_STR_EQ(buf, "test", "trims trailing spaces");

    strcpy(buf, "no_trail");
    ksys_trim_trailing_spaces(buf);
    ASSERT_STR_EQ(buf, "no_trail", "no trim needed");
}

int main(void) {
    test_version();
    test_uptime();
    test_memory_info();
    test_boot_info();
    test_cpu_info();
    test_u64_to_hex();
    test_copy_string();
    test_trim_trailing_spaces();
    TEST_RESULTS();
}
