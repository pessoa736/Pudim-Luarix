/*
 * ktimer_aarch64.c — ARM Generic Timer for aarch64
 *
 * Uses the ARM architectural timer (CNTPCT_EL0 / CNTFRQ_EL0)
 * instead of the x86 PIT/PIC.
 */
#include "ktimer.h"
#include "ksys.h"
#include "utypes.h"

static uint64_t g_timer_freq = 0;

static inline uint64_t arm_read_cntfrq(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

static inline uint64_t arm_read_cntpct(void) {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

void ktimer_init(unsigned int target_hz) {
    (void)target_hz;
    g_timer_freq = arm_read_cntfrq();
    if (g_timer_freq == 0) {
        g_timer_freq = 62500000; /* QEMU default: 62.5 MHz */
    }
}

uint64_t ktimer_uptime_ms(void) {
    if (g_timer_freq == 0) {
        return 0;
    }
    return (arm_read_cntpct() * 1000) / g_timer_freq;
}
