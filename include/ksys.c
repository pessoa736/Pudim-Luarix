#include "ksys.h"
#include "kheap.h"

/* Global system state */
static struct {
    uint64_t uptime_ms;
    uint64_t uptime_us;
} g_sysinfo = {0};

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
    /* 64MB default for QEMU */
    return 64 * 1024 * 1024;
}

uint64_t ksys_memory_free(void) {
    /* Approximate from heap */
    return kheap_free_bytes();
}

uint64_t ksys_memory_used(void) {
    return ksys_memory_total() - ksys_memory_free();
}

uint32_t ksys_cpu_count(void) {
    /* Single CPU for now (x86_64 baseline) */
    return 1;
}

void ksys_init(void) {
    g_sysinfo.uptime_ms = 0;
    g_sysinfo.uptime_us = 0;
}

void ksys_tick(void) {
    /* Assume 100Hz timer (10ms per tick) */
    g_sysinfo.uptime_ms += 10;
    g_sysinfo.uptime_us += 10000;
}
