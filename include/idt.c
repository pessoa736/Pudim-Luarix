#include "idt.h"
#include "kmemory.h"
#include "serial.h"

static void idt_print_hex_u64(uint64_t value) {
    char hex[] = "0123456789ABCDEF";
    int shift;

    vga_print("0x");

    for (shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> (uint64_t)shift) & 0xFull);
        char c[2];
        c[0] = hex[nibble];
        c[1] = 0;
        vga_print(c);
    }
}

static void idt_serial_print_hex_u64(uint64_t value) {
    char hex[] = "0123456789ABCDEF";
    int shift;

    serial_print("0x");

    for (shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> (uint64_t)shift) & 0xFull);
        serial_putchar(hex[nibble]);
    }
}

#define IDT_ENTRIES 256
#define KERNEL_CS 0x18

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

void set_idt_entry(void *isr, uint8_t flags, uint8_t index) {
    uint64_t addr;

    addr = (uint64_t)isr;

    idt[index].offset_low = (uint16_t)(addr & 0xFFFF);
    idt[index].selector = KERNEL_CS;
    idt[index].ist = 0;
    idt[index].type_attr = flags;
    idt[index].offset_mid = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[index].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[index].zero = 0;
}

void load_IDT(void) {
    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)idt;

    asm volatile ("lidt %0" : : "m"(idtr));
}

void division_error_handler(void){
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("Zero Division Error");

    while (1) {
        asm volatile ("hlt");
    }
}

void page_fault_handler(uint64_t fault_addr, uint64_t error_code){
    if (kmemory_handle_page_fault(fault_addr, error_code)) {
        return;
    }

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("Page Fault addr=");
    idt_print_hex_u64(fault_addr);
    vga_print(" err=");
    idt_print_hex_u64(error_code);
    vga_print("\n");

    serial_print("[pf] addr=");
    idt_serial_print_hex_u64(fault_addr);
    serial_print(" err=");
    idt_serial_print_hex_u64(error_code);
    serial_print("\r\n");

    while (1) {
        asm volatile ("hlt");
    }
}

/* ---- Stack backtrace ---- */
#define BACKTRACE_MAX_FRAMES 16

static void idt_backtrace_from_rbp(uint64_t rbp) {
    uint64_t* frame;
    int depth;

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("  Stack backtrace:\n");
    serial_print("  Stack backtrace:\r\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    frame = (uint64_t*)rbp;
    for (depth = 0; depth < BACKTRACE_MAX_FRAMES; depth++) {
        uint64_t next_rbp;
        uint64_t ret_addr;

        /* Basic pointer validation: must be canonical, aligned, and non-null */
        if ((uint64_t)frame < 0x1000 || ((uint64_t)frame & 0x7) != 0) {
            break;
        }

        next_rbp = frame[0];
        ret_addr = frame[1];

        if (ret_addr == 0) {
            break;
        }

        vga_print("    [");
        idt_print_hex_u64((uint64_t)(unsigned int)depth);
        vga_print("] ");
        idt_print_hex_u64(ret_addr);
        vga_print("\n");

        serial_print("    [");
        idt_serial_print_hex_u64((uint64_t)(unsigned int)depth);
        serial_print("] ");
        idt_serial_print_hex_u64(ret_addr);
        serial_print("\r\n");

        frame = (uint64_t*)next_rbp;
    }
}

/* ---- #GP: General Protection Fault (INT 13) ---- */

static void idt_print_gp_reason(uint64_t error_code) {
    if (error_code == 0) {
        vga_print("  Cause: unspecified (err=0)\n");
        serial_print("  Cause: unspecified (err=0)\r\n");
        return;
    }

    /* Error code bits: [15:3]=selector index, [2]=TI, [1]=IDT, [0]=EXT */
    vga_print("  Selector=");
    idt_print_hex_u64((error_code >> 3) & 0x1FFF);
    serial_print("  Selector=");
    idt_serial_print_hex_u64((error_code >> 3) & 0x1FFF);

    if (error_code & 0x1) {
        vga_print(" EXT");
        serial_print(" EXT");
    }

    if (error_code & 0x2) {
        vga_print(" IDT");
        serial_print(" IDT");
    } else if (error_code & 0x4) {
        vga_print(" LDT");
        serial_print(" LDT");
    } else {
        vga_print(" GDT");
        serial_print(" GDT");
    }

    vga_print("\n");
    serial_print("\r\n");
}

/*
 * Register order on stack (pushed in isr.asm):
 * r15 r14 r13 r12 r11 r10 r9 r8 rbp rbx rdi rsi rdx rcx rax
 * Index:  0    1    2    3   4   5  6  7   8   9  10  11  12  13  14
 */

static void idt_print_exception_regs(uint64_t rip, void* regs) {
    uint64_t* r = (uint64_t*)regs;

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  RIP=");
    idt_print_hex_u64(rip);
    vga_print("  RAX=");
    idt_print_hex_u64(r[14]);
    vga_print("\n");

    vga_print("  RBX=");
    idt_print_hex_u64(r[9]);
    vga_print("  RCX=");
    idt_print_hex_u64(r[13]);
    vga_print("\n");

    vga_print("  RDX=");
    idt_print_hex_u64(r[12]);
    vga_print("  RSI=");
    idt_print_hex_u64(r[11]);
    vga_print("\n");

    vga_print("  RDI=");
    idt_print_hex_u64(r[10]);
    vga_print("  RBP=");
    idt_print_hex_u64(r[8]);
    vga_print("\n");

    serial_print("  RIP=");
    idt_serial_print_hex_u64(rip);
    serial_print("  RAX=");
    idt_serial_print_hex_u64(r[14]);
    serial_print("\r\n");
    serial_print("  RBX=");
    idt_serial_print_hex_u64(r[9]);
    serial_print("  RCX=");
    idt_serial_print_hex_u64(r[13]);
    serial_print("\r\n");
    serial_print("  RDX=");
    idt_serial_print_hex_u64(r[12]);
    serial_print("  RSI=");
    idt_serial_print_hex_u64(r[11]);
    serial_print("\r\n");
    serial_print("  RDI=");
    idt_serial_print_hex_u64(r[10]);
    serial_print("  RBP=");
    idt_serial_print_hex_u64(r[8]);
    serial_print("\r\n");
}

void general_protection_handler(uint64_t error_code, uint64_t rip, void* regs) {
    uint64_t* r = (uint64_t*)regs;

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("General Protection Fault (#GP) err=");
    idt_print_hex_u64(error_code);
    vga_print("\n");

    serial_print("[exception] #GP err=");
    idt_serial_print_hex_u64(error_code);
    serial_print("\r\n");

    idt_print_gp_reason(error_code);
    idt_print_exception_regs(rip, regs);
    idt_backtrace_from_rbp(r[8]); /* r[8] = RBP */

    while (1) {
        asm volatile ("hlt");
    }
}

/* ---- #DF: Double Fault (INT 8) ---- */

void double_fault_handler(uint64_t error_code, uint64_t rip, void* regs) {
    (void)error_code; /* always 0 */

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("Double Fault (#DF)\n");

    serial_print("[exception] #DF\r\n");

    idt_print_exception_regs(rip, regs);

    /* #DF is an abort — no recovery possible */
    vga_print("  [ABORT] System halted\n");
    serial_print("  [ABORT] System halted\r\n");

    while (1) {
        asm volatile ("hlt");
    }
}