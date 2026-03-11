[BITS 32]
global entry
extern lau_main
extern __bss_start
extern __bss_end

entry:
    ; Write immediate marker to VGA to confirm entry point reached
    mov esi, 0xB8000      ; VGA memory base
    mov eax, 0x0F45       ; White 'E'
    mov [esi], ax         ; Write to first VGA position
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    mov esp, 0x600000
    
    ; Setup paging tables for long mode
    mov edi, 0x200000
    mov ecx, 12288 / 4
    xor eax, eax
    rep stosd
    
    mov dword [0x200000], 0x201000 | 3
    mov dword [0x201000], 0x202000 | 3
    
    ; Map first 1GB with 2MB pages
    mov edi, 0x202000
    mov ecx, 512
    mov eax, 0x83
.map_pages:
    mov dword [edi], eax
    mov dword [edi+4], 0
    add eax, 0x200000
    add edi, 8
    loop .map_pages
    
    mov eax, 0x200000
    mov cr3, eax
    
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax
    
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x900          ; LME | NXE
    wrmsr
    
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    jmp 0x18:entry64

[BITS 64]
entry64:
    cli

    mov ax, 0x20
    mov ds, ax
    mov ss, ax

    ; Enable x87/SSE for 64-bit C code (Lua uses floating-point paths).
    mov rax, cr0
    and rax, ~0x4          ; clear EM
    or rax, 0x2            ; set MP
    or rax, 0x10000        ; set WP to enforce write-protect in supervisor mode
    mov cr0, rax

    mov rax, cr4
    or rax, 0x600          ; OSFXSR | OSXMMEXCPT
    mov cr4, rax
    fninit

    ; Mask legacy PIC interrupts until a full IDT/IRQ setup exists.
    mov al, 0xFF
    out 0x21, al
    out 0xA1, al
    
    mov rsp, 0x500000
    
    ; Zero BSS
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    cld
    rep stosb
    
    call lau_main
    jmp $
