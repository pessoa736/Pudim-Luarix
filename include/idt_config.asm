[BITS 32]
ALIGN 8

extern __idt_start__
extern __idt_end__

global load_IDT
global set_idt_entry

KERNEL_CS equ 0x08

load_IDT:
    lidt [idtr]

    ret

set_idt_entry:
    push ebp
    mov ebp, esp

    mov eax, [ebp+8]
    mov ebx, [ebp+12]
    mov ecx, [ebp+16]

    mov word[__idt_start__ + ecx*8 + 0], ax
    mov word[__idt_start__ + ecx*8 + 2], KERNEL_CS
    mov byte[__idt_start__ + ecx*8 + 4], 0
    mov byte[__idt_start__ + ecx*8 + 5], bl
    shr eax, 16
    mov word[__idt_start__ + ecx*8 + 6], ax

    pop ebp
    ret

idtr:
    dw 256*8 -1
    dd __idt_start__