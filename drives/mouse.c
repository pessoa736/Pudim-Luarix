#include "mouse.h"
#include "utypes.h"

#define MOUSE_DATA_PORT   0x60
#define MOUSE_STATUS_PORT 0x64
#define MOUSE_CMD_PORT    0x64

#define PIC1_CMD  0x20
#define PIC2_CMD  0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

#define MOUSE_PACKET_SIZE 4

static inline void mouse_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint8_t mouse_inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

static void mouse_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((mouse_inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return;
        }
    }
}

static void mouse_wait_output(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (mouse_inb(MOUSE_STATUS_PORT) & 0x01) {
            return;
        }
    }
}

static void mouse_write_cmd(uint8_t cmd) {
    mouse_wait_input();
    mouse_outb(MOUSE_CMD_PORT, 0xD4);
    mouse_wait_input();
    mouse_outb(MOUSE_DATA_PORT, cmd);
}

static uint8_t mouse_read(void) {
    mouse_wait_output();
    return mouse_inb(MOUSE_DATA_PORT);
}

static void mouse_set_sample_rate(uint8_t rate) {
    mouse_write_cmd(0xF3);
    mouse_read(); /* ACK */
    mouse_write_cmd(rate);
    mouse_read(); /* ACK */
}

/* Accumulated scroll delta, consumed by mouse_get_scroll */
static volatile int g_scroll_delta = 0;

/* Packet state */
static uint8_t g_packet[MOUSE_PACKET_SIZE];
static int g_packet_idx = 0;
static int g_has_scroll_wheel = 0;

void mouse_init(void) {
    uint8_t status;
    uint8_t device_id;
    uint8_t mask;

    /* Enable auxiliary device (mouse) on PS/2 controller */
    mouse_wait_input();
    mouse_outb(MOUSE_CMD_PORT, 0xA8);

    /* Enable IRQ12: read controller config, set bit 1, write back */
    mouse_wait_input();
    mouse_outb(MOUSE_CMD_PORT, 0x20);
    mouse_wait_output();
    status = mouse_inb(MOUSE_DATA_PORT);
    status |= 0x02;  /* enable IRQ12 */
    status &= (uint8_t)~0x20u; /* enable mouse clock */
    mouse_wait_input();
    mouse_outb(MOUSE_CMD_PORT, 0x60);
    mouse_wait_input();
    mouse_outb(MOUSE_DATA_PORT, status);

    /* Use defaults */
    mouse_write_cmd(0xF6);
    mouse_read(); /* ACK */

    /* Enable scroll wheel: magic sample rate sequence 200, 100, 80 */
    mouse_set_sample_rate(200);
    mouse_set_sample_rate(100);
    mouse_set_sample_rate(80);

    /* Get device ID to check for scroll wheel */
    mouse_write_cmd(0xF2);
    mouse_read(); /* ACK */
    device_id = mouse_read();
    g_has_scroll_wheel = (device_id == 3 || device_id == 4);

    /* Enable data reporting */
    mouse_write_cmd(0xF4);
    mouse_read(); /* ACK */

    /* Unmask IRQ12 on slave PIC (bit 4 = IRQ12) */
    mask = mouse_inb(PIC2_DATA);
    mask &= (uint8_t)~0x10u;
    mouse_outb(PIC2_DATA, mask);

    /* Also unmask IRQ2 on master PIC (cascade) */
    mask = mouse_inb(PIC1_DATA);
    mask &= (uint8_t)~0x04u;
    mouse_outb(PIC1_DATA, mask);

    g_packet_idx = 0;
    g_scroll_delta = 0;
}

void mouse_irq_handler(void) {
    uint8_t byte;
    int packet_len;

    byte = mouse_inb(MOUSE_DATA_PORT);

    packet_len = g_has_scroll_wheel ? 4 : 3;

    /* Byte 0 must have bit 3 set (always-1 bit in PS/2 mouse protocol) */
    if (g_packet_idx == 0 && (byte & 0x08) == 0) {
        /* Out of sync, discard */
        mouse_outb(PIC2_CMD, PIC_EOI);
        mouse_outb(PIC1_CMD, PIC_EOI);
        return;
    }

    if (g_packet_idx < MOUSE_PACKET_SIZE) {
        g_packet[g_packet_idx] = byte;
    }
    g_packet_idx++;

    if (g_packet_idx >= packet_len) {
        g_packet_idx = 0;

        if (g_has_scroll_wheel) {
            int z = (int)(signed char)g_packet[3];
            if (z != 0) {
                g_scroll_delta += z;
            }
        }
    }

    /* Send EOI to both PICs (slave IRQ) */
    mouse_outb(PIC2_CMD, PIC_EOI);
    mouse_outb(PIC1_CMD, PIC_EOI);
}

int mouse_get_scroll(void) {
    int delta;

    delta = g_scroll_delta;
    g_scroll_delta = 0;

    return delta;
}
