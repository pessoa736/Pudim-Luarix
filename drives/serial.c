#include "serial.h"

#define SERIAL_PORT 0x3F8
#define LSR_TRANSMIT_EMPTY 0x20
#define LSR_DATA_READY 0x01
#define SERIAL_TX_SPIN_MAX 1000000u

#define SERIAL_RX_BUF_SIZE 256u

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC_EOI   0x20

static volatile char g_serial_rx_buf[SERIAL_RX_BUF_SIZE];
static volatile uint16_t g_serial_rx_head = 0;
static volatile uint16_t g_serial_rx_tail = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts first */
    outb(SERIAL_PORT + 3, 0x80);  /* DLAB */
    outb(SERIAL_PORT + 0, 1);     /* Low divisor (115200 baud) */
    outb(SERIAL_PORT + 1, 0);     /* High divisor */
    outb(SERIAL_PORT + 3, 0x03);  /* No DLAB, 8N1 */
    outb(SERIAL_PORT + 2, 0xC7);  /* FIFO enable, clear, 14-byte threshold */
    outb(SERIAL_PORT + 4, 0x0B);  /* MCR: DTR + RTS + OUT2 */
}

void serial_init_irq(void) {
    uint8_t mask;

    /* Enable Received Data Available interrupt (IER bit 0) */
    outb(SERIAL_PORT + 1, 0x01);

    /* Unmask IRQ4 on PIC1 (bit 4) */
    mask = inb(PIC1_DATA);
    mask &= (uint8_t)~0x10u;
    outb(PIC1_DATA, mask);
}

void serial_putchar(char c) {
    uint32_t spins = 0;

    while ((inb(SERIAL_PORT + 5) & LSR_TRANSMIT_EMPTY) == 0) {
        spins++;
        if (spins >= SERIAL_TX_SPIN_MAX) {
            return;
        }
    }

    outb(SERIAL_PORT + 0, (uint8_t)c);
}

void serial_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        serial_putchar(s[i]);
    }
}

int serial_can_read(void) {
    /* Check ring buffer first, then hardware */
    if (g_serial_rx_head != g_serial_rx_tail) {
        return 1;
    }
    return (inb(SERIAL_PORT + 5) & LSR_DATA_READY) != 0;
}

char serial_getchar(void) {
    /* Try ring buffer first */
    while (g_serial_rx_head == g_serial_rx_tail) {
        /* Fallback: poll hardware directly */
        if (inb(SERIAL_PORT + 5) & LSR_DATA_READY) {
            return (char)inb(SERIAL_PORT + 0);
        }
    }

    {
        char c;
        uint16_t tail = g_serial_rx_tail;
        c = g_serial_rx_buf[tail];
        g_serial_rx_tail = (uint16_t)((tail + 1) % SERIAL_RX_BUF_SIZE);
        return c;
    }
}

int serial_read_nonblock(char* out) {
    if (!out) {
        return 0;
    }

    if (g_serial_rx_head != g_serial_rx_tail) {
        uint16_t tail = g_serial_rx_tail;
        *out = g_serial_rx_buf[tail];
        g_serial_rx_tail = (uint16_t)((tail + 1) % SERIAL_RX_BUF_SIZE);
        return 1;
    }

    if (inb(SERIAL_PORT + 5) & LSR_DATA_READY) {
        *out = (char)inb(SERIAL_PORT + 0);
        return 1;
    }

    return 0;
}

void serial_irq_handler(void) {
    /* Drain all available bytes from FIFO into ring buffer */
    while (inb(SERIAL_PORT + 5) & LSR_DATA_READY) {
        char c = (char)inb(SERIAL_PORT + 0);
        uint16_t next = (uint16_t)((g_serial_rx_head + 1) % SERIAL_RX_BUF_SIZE);

        if (next != g_serial_rx_tail) {
            g_serial_rx_buf[g_serial_rx_head] = c;
            g_serial_rx_head = next;
        }
        /* If buffer full, drop the byte */
    }

    outb(PIC1_CMD, PIC_EOI);
}
