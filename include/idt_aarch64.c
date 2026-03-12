/*
 * idt_aarch64.c — Interrupt stubs for aarch64
 *
 * aarch64 uses GIC + exception vectors instead of x86 IDT.
 * For now, minimal stubs to satisfy the API.
 */
#include "idt.h"
#include "utypes.h"
#include "arch.h"

/* Stub exception vector addresses */
static int g_handler_installed[256];

void set_idt_entry(void* handler, uint8_t type_attr, uint8_t vector) {
    (void)handler;
    (void)type_attr;
    if (vector < 255) {
        g_handler_installed[vector] = 1;
    }
}

void load_IDT(void) {
    /* No IDT on aarch64 — exception vectors set up in boot stub */
}

int idt_entry_present(uint8_t index) {
    return g_handler_installed[index];
}
