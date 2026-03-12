/*
 * arch_x86_64.h — x86_64 architecture primitives
 * Included by arch.h when compiling for x86_64.
 */
#ifndef ARCH_X86_64_H
#define ARCH_X86_64_H

#include "utypes.h"

static inline void arch_halt(void) {
    asm volatile("hlt");
}

static inline void arch_nop(void) {
    asm volatile("nop");
}

static inline void arch_enable_interrupts(void) {
    asm volatile("sti");
}

static inline void arch_disable_interrupts(void) {
    asm volatile("cli");
}

static inline void arch_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint8_t arch_inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

static inline void arch_pause(void) {
    asm volatile("pause");
}

#endif
