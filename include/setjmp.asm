[BITS 64]

; x86_64 setjmp/longjmp for Lua error recovery
; jmp_buf layout (first 64 bytes of system jmp_buf):
;   [0]  rbx
;   [8]  rbp
;   [16] r12
;   [24] r13
;   [32] r14
;   [40] r15
;   [48] rsp (caller's stack pointer after return)
;   [56] rip (return address)

global setjmp
global _setjmp
global longjmp
global _longjmp

setjmp:
_setjmp:
    mov [rdi],      rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    lea rax, [rsp + 8]
    mov [rdi + 48], rax
    mov rax, [rsp]
    mov [rdi + 56], rax
    xor eax, eax
    ret

longjmp:
_longjmp:
    mov rbx, [rdi]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 32]
    mov r15, [rdi + 40]
    mov rsp, [rdi + 48]
    mov eax, esi
    test eax, eax
    jnz .done
    inc eax
.done:
    jmp qword [rdi + 56]
