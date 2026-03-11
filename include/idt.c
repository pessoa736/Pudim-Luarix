#include "idt.h"

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