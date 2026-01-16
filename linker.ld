ENTRY(entry)

SECTIONS
{
    . = 0x1000;

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

    .idt :
    {
        __idt_start__ =  0xF000;
         
    }
}
