/*
 * boot_aarch64.S — Pudding Boot for aarch64 (QEMU virt machine)
 *
 * Minimal entry point: sets up stack and jumps to lau_main().
 * No paging setup needed — QEMU virt loads the kernel flat.
 */
.section .text
.global _start
.global entry

entry:
_start:
    /* Disable interrupts */
    msr daifset, #0xF

    /* Get current CPU ID — only CPU 0 boots */
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbnz x0, .hang

    /* Set up stack at 2MB (grows downward) */
    ldr x0, =0x200000
    mov sp, x0

    /* Zero BSS */
    ldr x0, =__bss_start
    ldr x1, =__bss_end
.bss_loop:
    cmp x0, x1
    b.ge .bss_done
    str xzr, [x0], #8
    b .bss_loop
.bss_done:

    /* Jump to kernel */
    bl lau_main

.hang:
    wfi
    b .hang
