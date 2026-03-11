#include "ktimer.h"
#include "kprocess.h"
#include "ksys.h"

#define PIT_INPUT_HZ 1193182u

#define PIT_CH0_DATA 0x40
#define PIT_MODE_CMD 0x43

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

static inline void ktimer_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint8_t ktimer_inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}

static void ktimer_pic_remap(void) {
    uint8_t mask1 = ktimer_inb(PIC1_DATA);
    uint8_t mask2 = ktimer_inb(PIC2_DATA);

    ktimer_outb(PIC1_CMD, 0x11);
    ktimer_outb(PIC2_CMD, 0x11);

    ktimer_outb(PIC1_DATA, 0x20);
    ktimer_outb(PIC2_DATA, 0x28);

    ktimer_outb(PIC1_DATA, 0x04);
    ktimer_outb(PIC2_DATA, 0x02);

    ktimer_outb(PIC1_DATA, 0x01);
    ktimer_outb(PIC2_DATA, 0x01);

    ktimer_outb(PIC1_DATA, mask1);
    ktimer_outb(PIC2_DATA, mask2);
}

static void ktimer_pit_program(uint32_t hz) {
    uint32_t divisor;

    if (hz == 0) {
        hz = 100;
    }

    divisor = PIT_INPUT_HZ / hz;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 0xFFFFu) {
        divisor = 0xFFFFu;
    }

    ktimer_outb(PIT_MODE_CMD, 0x36);
    ktimer_outb(PIT_CH0_DATA, (uint8_t)(divisor & 0xFFu));
    ktimer_outb(PIT_CH0_DATA, (uint8_t)((divisor >> 8) & 0xFFu));
}

void ktimer_init(uint32_t hz) {
    uint8_t mask1;

    ktimer_pic_remap();
    ktimer_pit_program(hz);

    mask1 = ktimer_inb(PIC1_DATA);
    mask1 &= (uint8_t)~0x01u;
    ktimer_outb(PIC1_DATA, mask1);
}

void ktimer_irq_handler(void) {
    ksys_tick();
    kprocess_request_tick();
    ktimer_outb(PIC1_CMD, PIC_EOI);
}