#include "serial.h"

#define SERIAL_PORT 0x3F8
#define LSR_TRANSMIT_EMPTY 0x20
#define LSR_DATA_READY 0x01

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

void serial_init(void) {
    outb(SERIAL_PORT + 3, 0x80);  /* DLAB */
    outb(SERIAL_PORT + 0, 1);     /* Low divisor */
    outb(SERIAL_PORT + 1, 0);     /* High divisor */
    outb(SERIAL_PORT + 3, 0x03);  /* No DLAB, 8 bits */
    outb(SERIAL_PORT + 2, 0xC7);  /* FIFO */
    outb(SERIAL_PORT + 4, 0x0B);  /* MCR */
}

void serial_putchar(char c) {
    while ((inb(SERIAL_PORT + 5) & LSR_TRANSMIT_EMPTY) == 0);
    outb(SERIAL_PORT + 0, (uint8_t)c);
}

void serial_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        serial_putchar(s[i]);
    }
}

int serial_can_read(void) {
    return (inb(SERIAL_PORT + 5) & LSR_DATA_READY) != 0;
}

char serial_getchar(void) {
    while (!serial_can_read()) {
    }

    return (char)inb(SERIAL_PORT + 0);
}
