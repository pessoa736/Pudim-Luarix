[BITS 16]
[ORG 0x7C00]

KERNEL_LOCATION equ 0x10000
DATA_SEG equ 0x10
CODE_SEG equ 0x08
CODE64_SEG equ 0x18
DATA64_SEG equ 0x20

PML4_TABLE equ 0x70000
PDPT_TABLE equ 0x71000
PD_TABLE   equ 0x72000

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 16
%endif

; Jump to main bootloader code
jmp start

print_hex_nibble:
    and al, 0x0F
    cmp al, 9
    jbe .digit
    add al, 'A' - 10
    jmp .emit

.digit:
    add al, '0'

.emit:
    mov ah, 0x0E
    int 0x10
    ret

print_hex8:
    push bx

    mov bl, al
    shr al, 4
    call print_hex_nibble

    mov al, bl
    call print_hex_nibble

    pop bx
    ret

start:
    cli
    mov [boot_drive], dl

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Ultra-early VRAM test (shows if we reach bootloader at all)
    mov ax, 0xB800
    mov gs, ax
    mov byte [gs:0x0000], '*'
    mov byte [gs:0x0001], 0x0F

    mov ah, 0x0E
    mov al, 'R'
    int 0x10

    in al, 0x92
    or al, 00000010b
    out 0x92, al

load_kernel:
    mov word [dap + 2], 0
    mov word [dap + 4], 0x0000
    mov word [dap + 6], (KERNEL_LOCATION >> 4)
    mov dword [dap + 8], 1
    mov dword [dap + 12], 0

    mov cx, KERNEL_SECTORS

.load_loop:
    cmp cx, 0
    je .load_done

    mov ax, cx
    cmp ax, 32
    jbe .chunk_ok
    mov ax, 32

.chunk_ok:
    mov [dap + 2], ax

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc load_kernel_error

    ; avanca LBA
    add word [dap + 8], ax
    adc word [dap + 10], 0
    adc word [dap + 12], 0
    adc word [dap + 14], 0

    ; avanca destino em paragrafos (setores * 512 / 16 = setores * 32)
    mov bx, ax
    shl bx, 5
    add word [dap + 6], bx

    sub cx, ax
    jmp .load_loop

.load_done:
    mov ah, 0x0E
    mov al, 'D'
    int 0x10

    cli
    
    ; Load GDT directly using the descriptor label
    lgdt [gdt_descriptor]
    
    mov ah, 0x0E
    mov al, 'G'
    int 0x10
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to kernel in protected mode
    jmp dword CODE_SEG:0x10000

; Boot drive is saved here
boot_drive: db 0

; Disk Address Packet for LBA read
dap:
    db 0x10
    db 0
    dw KERNEL_SECTORS
    dw 0x0000
    dw 0x0000
    dq 1

[BITS 16]
load_kernel_error:
    mov bl, ah

    mov ah, 0x0E
    mov al, 'E'
    int 0x10

    mov al, ' '
    int 0x10

    mov al, bl
    call print_hex8

    jmp $

; GDT Table
align 8
gdt_start:
    dq 0    ; Null selector

gdt_code:
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base mid
    db 10011010b    ; Attributes
    db 11001111b    ; Flags and limit high
    db 0x00         ; Base high

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_code64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 00100000b
    db 0x00

gdt_data64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00

gdt_end:

; GDT Descriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1     ; Size
    dd gdt_start                    ; Base address

; Boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
