#include "include/vga.h"
#include "include/idt.h"

void lau_main() {
    set_idt_entry(&ir0, (uint8_t)0x8E, (uint8_t)0);
    load_IDT();

    int zero_div = 10/0;
    zero_div = 0;

    while(1);
}
