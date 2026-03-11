[BITS 16]
[ORG 0x7C00]

KERNEL_LOCATION equ 0x10000
DATA_SEG equ 0x10
CODE_SEG equ 0x08
CODE64_SEG equ 0x18
DATA64_SEG equ 0x20

PML4_TABLE equ 0x200000
PDPT_TABLE equ 0x201000
PD_TABLE   equ 0x202000
BOOT_INFO_ADDR equ 0x5000
E820_BUFFER equ 0x5100
DRIVE_PARAMS_BUFFER equ 0x5200

BOOT_INFO_MAGIC equ 0x584C4450
BOOT_INFO_MAGIC_OFF equ 0
BOOT_INFO_RAM_USABLE_LOW_OFF equ 4
BOOT_INFO_RAM_USABLE_HIGH_OFF equ 8
BOOT_INFO_BOOT_SECTORS_LOW_OFF equ 12
BOOT_INFO_BOOT_SECTORS_HIGH_OFF equ 16
BOOT_INFO_SECTOR_SIZE_OFF equ 20
BOOT_INFO_E820_COUNT_OFF equ 22
BOOT_INFO_E820_ENTRY_SIZE_OFF equ 24
BOOT_INFO_E820_MAX_ENTRIES_OFF equ 26
BOOT_INFO_KERNEL_BYTES_LOW_OFF equ 32
BOOT_INFO_KERNEL_BYTES_HIGH_OFF equ 36
BOOT_INFO_HEADER_SIZE equ 40
BOOT_INFO_E820_MAX_ENTRIES equ 32
BOOT_INFO_E820_ENTRY_SIZE equ 24
BOOT_INFO_E820_ADDR equ 0x5300

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 16
%endif

%ifndef KERNEL_BYTES
%define KERNEL_BYTES (KERNEL_SECTORS * 512)
%endif

; Jump to main bootloader code
jmp start

boot_info_init:
    push ax
    push cx
    push di

    xor ax, ax
    mov di, BOOT_INFO_ADDR
    mov cx, BOOT_INFO_HEADER_SIZE
    cld
    rep stosb

    mov dword [BOOT_INFO_ADDR + BOOT_INFO_MAGIC_OFF], BOOT_INFO_MAGIC
    mov word [BOOT_INFO_ADDR + BOOT_INFO_E820_ENTRY_SIZE_OFF], BOOT_INFO_E820_ENTRY_SIZE
    mov word [BOOT_INFO_ADDR + BOOT_INFO_E820_MAX_ENTRIES_OFF], BOOT_INFO_E820_MAX_ENTRIES
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_KERNEL_BYTES_LOW_OFF], KERNEL_BYTES
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_KERNEL_BYTES_HIGH_OFF], 0

    pop di
    pop cx
    pop ax
    ret

boot_info_detect_memory:
    pushad

    xor ebx, ebx
    xor bp, bp

.next_entry:
    mov di, E820_BUFFER
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov dword [di + 20], 1
    int 0x15
    jc .done

    cmp eax, 0x534D4150
    jne .done

    cmp bp, BOOT_INFO_E820_MAX_ENTRIES
    jae .skip_store

    movzx eax, bp
    imul eax, eax, BOOT_INFO_E820_ENTRY_SIZE
    mov edi, BOOT_INFO_E820_ADDR
    add edi, eax
    mov esi, E820_BUFFER
    mov ecx, BOOT_INFO_E820_ENTRY_SIZE / 4
    cld
    rep movsd

    inc bp
    mov [BOOT_INFO_ADDR + BOOT_INFO_E820_COUNT_OFF], bp

.skip_store:
    cmp dword [E820_BUFFER + 16], 1
    jne .continue

    mov eax, dword [E820_BUFFER + 8]
    add dword [BOOT_INFO_ADDR + BOOT_INFO_RAM_USABLE_LOW_OFF], eax
    mov eax, dword [E820_BUFFER + 12]
    adc dword [BOOT_INFO_ADDR + BOOT_INFO_RAM_USABLE_HIGH_OFF], eax

.continue:
    cmp ebx, 0
    jne .next_entry

.done:
    popad
    ret

boot_info_detect_drive_size:
    pushad

    xor ax, ax
    mov di, DRIVE_PARAMS_BUFFER
    mov cx, 64
    cld
    rep stosb

    mov word [DRIVE_PARAMS_BUFFER], 0x1E
    mov dl, [boot_drive]
    mov si, DRIVE_PARAMS_BUFFER
    mov ah, 0x48
    int 0x13
    jc .done

    mov eax, dword [DRIVE_PARAMS_BUFFER + 16]
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_BOOT_SECTORS_LOW_OFF], eax
    mov eax, dword [DRIVE_PARAMS_BUFFER + 20]
    mov dword [BOOT_INFO_ADDR + BOOT_INFO_BOOT_SECTORS_HIGH_OFF], eax
    mov ax, word [DRIVE_PARAMS_BUFFER + 24]
    mov word [BOOT_INFO_ADDR + BOOT_INFO_SECTOR_SIZE_OFF], ax

.done:
    popad
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

    call boot_info_init
    call boot_info_detect_memory
    call boot_info_detect_drive_size

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
    mov ah, 0x0E
    mov al, 'E'
    int 0x10

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
