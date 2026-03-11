#include "idt.h"
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