/*
 * kdisplay.c — PV: Pudim na Vitrine (Pudding in the Display Case)
 *
 * Auto-detects whether a VGA monitor is present. On x86_64 with
 * VGA, output goes to VGA text mode and mirrors to serial. Without
 * VGA (aarch64, or x86 without monitor), everything goes through
 * serial — the kitchen window — so you can use it from Termux.
 */
#include "kdisplay.h"
#include "serial.h"
#include "arch.h"

#if ARCH_HAS_VGA
#include "vga.h"
#endif

static int g_display_mode = KDISPLAY_MODE_SERIAL;

/*
 * VGA probe: on x86_64, try to write and read back a marker at
 * VGA memory 0xB8000. If it echoes, a VGA display is present.
 * On aarch64, VGA is never available.
 */
static int kdisplay_probe_vga(void) {
#if ARCH_HAS_VGA
    volatile uint16_t* vga_mem = (volatile uint16_t*)0xB8000;
    uint16_t saved = vga_mem[0];
    uint16_t marker = (uint16_t)((0x07u << 8) | 0xAA);

    vga_mem[0] = marker;

    if (vga_mem[0] == marker) {
        vga_mem[0] = saved;
        return 1;
    }

    return 0;
#else
    return 0;
#endif
}

void kdisplay_init(void) {
    if (kdisplay_probe_vga()) {
        g_display_mode = KDISPLAY_MODE_VGA;
    } else {
        g_display_mode = KDISPLAY_MODE_SERIAL;
    }
}

int kdisplay_mode(void) {
    return g_display_mode;
}

void kdisplay_write(const char* s) {
    if (!s) {
        return;
    }

#if ARCH_HAS_VGA
    if (g_display_mode == KDISPLAY_MODE_VGA) {
        vga_print(s);
        return;
    }
#endif

    serial_print(s);
}

void kdisplay_clear(void) {
#if ARCH_HAS_VGA
    if (g_display_mode == KDISPLAY_MODE_VGA) {
        vga_clear();
        return;
    }
#endif

    /* Serial clear: send ANSI escape to clear terminal */
    serial_print("\033[2J\033[H");
}

void kdisplay_putchar(char c) {
#if ARCH_HAS_VGA
    if (g_display_mode == KDISPLAY_MODE_VGA) {
        vga_putchar(c);
        return;
    }
#endif

    serial_putchar(c);
}
