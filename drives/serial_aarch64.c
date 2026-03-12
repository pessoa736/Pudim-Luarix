/*
 * serial_aarch64.c — PL011 UART driver for aarch64 (QEMU virt)
 *
 * On aarch64, serial uses MMIO at 0x09000000 instead of x86 port I/O.
 * Implements the same serial.h API so the rest of the kernel is unchanged.
 */
#include "serial.h"
#include "arch.h"

#define PL011_BASE   0x09000000u

/* PL011 register offsets */
#define PL011_DR     0x000  /* Data Register */
#define PL011_FR     0x018  /* Flag Register */
#define PL011_IBRD   0x024  /* Integer Baud Rate Divisor */
#define PL011_FBRD   0x028  /* Fractional Baud Rate Divisor */
#define PL011_LCRH   0x02C  /* Line Control */
#define PL011_CR     0x030  /* Control Register */
#define PL011_IMSC   0x038  /* Interrupt Mask Set/Clear */
#define PL011_ICR    0x044  /* Interrupt Clear */

/* Flag bits */
#define PL011_FR_TXFF  (1u << 5)  /* TX FIFO full */
#define PL011_FR_RXFE  (1u << 4)  /* RX FIFO empty */
#define PL011_FR_BUSY  (1u << 3)

/* Control bits */
#define PL011_CR_UARTEN (1u << 0)
#define PL011_CR_TXE    (1u << 8)
#define PL011_CR_RXE    (1u << 9)

/* Line control bits */
#define PL011_LCRH_WLEN8 (3u << 5)  /* 8-bit word */
#define PL011_LCRH_FEN   (1u << 4)  /* FIFO enable */

#define SERIAL_RX_BUF_SIZE 256u

static volatile char g_serial_rx_buf[SERIAL_RX_BUF_SIZE];
static volatile uint16_t g_serial_rx_head = 0;
static volatile uint16_t g_serial_rx_tail = 0;

static inline void pl011_write(uint32_t offset, uint32_t val) {
    arch_mmio_write32(PL011_BASE + offset, val);
}

static inline uint32_t pl011_read(uint32_t offset) {
    return arch_mmio_read32(PL011_BASE + offset);
}

void serial_init(void) {
    /* Disable UART */
    pl011_write(PL011_CR, 0);

    /* Clear interrupts */
    pl011_write(PL011_ICR, 0x7FF);

    /* Set baud rate: 115200 at 24MHz clock
       IBRD = 24000000 / (16 * 115200) = 13
       FBRD = ((0.020833 * 64) + 0.5) = 1 */
    pl011_write(PL011_IBRD, 13);
    pl011_write(PL011_FBRD, 1);

    /* 8N1, FIFO enabled */
    pl011_write(PL011_LCRH, PL011_LCRH_WLEN8 | PL011_LCRH_FEN);

    /* Enable UART, TX, RX */
    pl011_write(PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

void serial_init_irq(void) {
    /* Enable RX interrupt */
    pl011_write(PL011_IMSC, (1u << 4));
}

void serial_putchar(char c) {
    uint32_t spins = 0;

    /* Wait until TX FIFO has space */
    while (pl011_read(PL011_FR) & PL011_FR_TXFF) {
        spins++;
        if (spins >= 1000000u) {
            return;
        }
    }

    pl011_write(PL011_DR, (uint32_t)(uint8_t)c);
}

void serial_print(const char* s) {
    int i;

    if (!s) {
        return;
    }

    for (i = 0; s[i]; i++) {
        serial_putchar(s[i]);
    }
}

int serial_can_read(void) {
    if (g_serial_rx_head != g_serial_rx_tail) {
        return 1;
    }

    return (pl011_read(PL011_FR) & PL011_FR_RXFE) == 0;
}

char serial_getchar(void) {
    /* Try ring buffer first */
    while (g_serial_rx_head == g_serial_rx_tail) {
        if ((pl011_read(PL011_FR) & PL011_FR_RXFE) == 0) {
            return (char)(pl011_read(PL011_DR) & 0xFF);
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

    if ((pl011_read(PL011_FR) & PL011_FR_RXFE) == 0) {
        *out = (char)(pl011_read(PL011_DR) & 0xFF);
        return 1;
    }

    return 0;
}

void serial_irq_handler(void) {
    /* Drain all available bytes from FIFO into ring buffer */
    while ((pl011_read(PL011_FR) & PL011_FR_RXFE) == 0) {
        char c = (char)(pl011_read(PL011_DR) & 0xFF);
        uint16_t next = (uint16_t)((g_serial_rx_head + 1) % SERIAL_RX_BUF_SIZE);

        if (next != g_serial_rx_tail) {
            g_serial_rx_buf[g_serial_rx_head] = c;
            g_serial_rx_head = next;
        }
    }

    /* Clear RX interrupt */
    pl011_write(PL011_ICR, (1u << 4));
}
