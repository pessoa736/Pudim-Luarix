/*
 * arch_aarch64.h — aarch64 architecture primitives
 * Included by arch.h when compiling for aarch64.
 */
#ifndef ARCH_AARCH64_H
#define ARCH_AARCH64_H

#include "utypes.h"

static inline void arch_halt(void) {
    asm volatile("wfi");
}

static inline void arch_nop(void) {
    asm volatile("nop");
}

static inline void arch_enable_interrupts(void) {
    asm volatile("msr daifclr, #0xF");
}

static inline void arch_disable_interrupts(void) {
    asm volatile("msr daifset, #0xF");
}

/*
 * aarch64 has no port I/O — use MMIO instead.
 * These map port addresses to memory-mapped volatile accesses.
 * For PL011 UART the base is 0x09000000 on QEMU virt.
 */
static inline void arch_outb(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
    /* No-op: aarch64 drivers use MMIO directly */
}

static inline uint8_t arch_inb(uint16_t port) {
    (void)port;
    return 0;
}

/* MMIO helpers for aarch64 */
static inline void arch_mmio_write32(uint64_t addr, uint32_t val) {
    *(volatile uint32_t*)(addr) = val;
}

static inline uint32_t arch_mmio_read32(uint64_t addr) {
    return *(volatile uint32_t*)(addr);
}

static inline void arch_mmio_write8(uint64_t addr, uint8_t val) {
    *(volatile uint8_t*)(addr) = val;
}

static inline uint8_t arch_mmio_read8(uint64_t addr) {
    return *(volatile uint8_t*)(addr);
}

static inline void arch_pause(void) {
    asm volatile("yield");
}

#endif
