[BITS 64]
extern division_error_handler
extern general_protection_handler
extern double_fault_handler
extern ktimer_irq_handler
extern page_fault_handler
extern serial_irq_handler
extern mouse_irq_handler
global ir0
global ir8
global ir13
global irq0
global irq4
global irq12
global ir14

ir0:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call division_error_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

ir8:
    ; #DF pushes error code (always 0)
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbx
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 120]    ; error code (always 0)
    mov rsi, [rsp + 128]    ; RIP at fault
    mov rdx, rsp            ; pointer to saved regs

    call double_fault_handler

    ; #DF is abort — never returns
    hlt
    jmp $

ir13:
    ; #GP pushes error code
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push rbx
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 120]    ; error code
    mov rsi, [rsp + 128]    ; RIP at fault
    mov rdx, rsp            ; pointer to saved regs

    call general_protection_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rbx
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    add rsp, 8              ; pop error code
    iretq

ir14:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rdi, cr2
    mov rsi, [rsp + 72]

    call page_fault_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    add rsp, 8
    iretq

irq0:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call ktimer_irq_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

irq4:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call serial_irq_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

irq12:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call mouse_irq_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq