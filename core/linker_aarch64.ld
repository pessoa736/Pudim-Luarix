/*
 * linker_aarch64.ld — Linker script for aarch64 (QEMU virt)
 * Kernel loaded at 0x40080000 by QEMU -kernel
 */
ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text :
    {
        *(.text*)
    }

    .rodata :
    {
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
    }

    .bss :
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_end = .;
}
