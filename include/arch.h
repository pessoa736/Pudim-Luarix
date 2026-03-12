/*
 * arch.h — Camada de Arquitetura (Architecture Layer)
 *
 * Like choosing the mold shape for the pudding — the recipe
 * (kernel) stays the same, but the mold (arch) determines
 * how it bakes. x86_64 uses a rectangular tin; aarch64 uses
 * a rounded bundt pan.
 *
 * Provides a unified API for architecture-specific primitives:
 *   CPU control, I/O ports (x86) or MMIO (ARM), interrupts.
 */
#ifndef ARCH_H
#define ARCH_H

#include "utypes.h"

#if defined(__x86_64__) || defined(__i386__)
    #define ARCH_X86_64 1
    #include "arch_x86_64.h"
#elif defined(__aarch64__)
    #define ARCH_AARCH64 1
    #include "arch_aarch64.h"
#else
    #error "Unsupported architecture"
#endif

/* --- Common API (implemented per-arch) --- */

/* CPU control */
static inline void arch_halt(void);
static inline void arch_nop(void);
static inline void arch_enable_interrupts(void);
static inline void arch_disable_interrupts(void);

/* Port I/O (x86) or MMIO stubs (aarch64) */
static inline void arch_outb(uint16_t port, uint8_t val);
static inline uint8_t arch_inb(uint16_t port);

/* UART base address for serial */
#ifdef ARCH_X86_64
    #define ARCH_SERIAL_BASE 0x3F8u
#endif
#ifdef ARCH_AARCH64
    #define ARCH_SERIAL_BASE 0x09000000u  /* QEMU virt PL011 */
#endif

/* VGA availability */
#ifdef ARCH_X86_64
    #define ARCH_HAS_VGA 1
#else
    #define ARCH_HAS_VGA 0
#endif

/* Architecture name string */
#ifdef ARCH_X86_64
    #define ARCH_NAME "x86_64"
#endif
#ifdef ARCH_AARCH64
    #define ARCH_NAME "aarch64"
#endif

#endif
