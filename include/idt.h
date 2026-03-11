#ifndef IDT_H
#define IDT_H

#include "utypes.h"
#include "vga.h"

void load_IDT(void);
void set_idt_entry(void *isr, uint8_t flags, uint8_t index);

extern void ir0(void);

void division_error_handler(void);


#endif