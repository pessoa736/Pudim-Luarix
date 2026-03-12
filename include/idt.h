#ifndef IDT_H
#define IDT_H

#include "utypes.h"
#include "vga.h"

void load_IDT(void);
void set_idt_entry(void *isr, uint8_t flags, uint8_t index);

extern void ir0(void);
extern void ir8(void);
extern void ir13(void);
extern void irq0(void);
extern void irq4(void);
extern void ir14(void);

void division_error_handler(void);
void general_protection_handler(uint64_t error_code, uint64_t rip, void* regs);
void double_fault_handler(uint64_t error_code, uint64_t rip, void* regs);
void page_fault_handler(uint64_t fault_addr, uint64_t error_code);


#endif