#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5

#define VGA_SCROLLBACK_LINES 200

static volatile uint16_t* const vga = (uint16_t*)0xB8000;
static uint8_t current_color = (VGA_WHITE | (VGA_BLACK << 4));

static uint16_t cursor;

/* Scrollback buffer: stores VGA attribute+char pairs for each cell */
static uint16_t scrollback[VGA_SCROLLBACK_LINES][VGA_WIDTH];
static int scrollback_count = 0;   /* total lines stored */
static int scrollback_head = 0;    /* next write slot (ring) */
static int scroll_offset = 0;      /* how many lines scrolled back from live */
/* Saved live screen when user is scrolled back */
static uint16_t live_screen[VGA_HEIGHT * VGA_WIDTH];
static uint16_t live_cursor;

static inline void vga_outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "d"(port));
}

static void vga_update_hw_cursor(void) {
    vga_outb(VGA_CRTC_INDEX, 0x0F);
    vga_outb(VGA_CRTC_DATA, (uint8_t)(cursor & 0xFF));
    vga_outb(VGA_CRTC_INDEX, 0x0E);
    vga_outb(VGA_CRTC_DATA, (uint8_t)((cursor >> 8) & 0xFF));
}

static void vga_scroll_up_one(void) {
    uint16_t blank = (current_color << 8) | ' ';
    uint16_t total = VGA_WIDTH * VGA_HEIGHT;
    uint16_t last_row_start = total - VGA_WIDTH;
    uint16_t i;

    /* Save the top line to scrollback before discarding */
    {
        int col;
        for (col = 0; col < VGA_WIDTH; col++) {
            scrollback[scrollback_head][col] = vga[col];
        }
        scrollback_head = (scrollback_head + 1) % VGA_SCROLLBACK_LINES;
        if (scrollback_count < VGA_SCROLLBACK_LINES) {
            scrollback_count++;
        }
    }

    for (i = 0; i < last_row_start; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }

    for (i = last_row_start; i < total; i++) {
        vga[i] = blank;
    }
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = fg | (bg << 4);
}

void vga_clear(void) {
    uint16_t blank = (current_color << 8) | ' ';

    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = blank;
    }

    cursor = 0;
    scroll_offset = 0;
    scrollback_count = 0;
    scrollback_head = 0;
    vga_update_hw_cursor();
}

/* ANSI escape sequence parser state */
#define ANSI_STATE_NORMAL  0
#define ANSI_STATE_ESC     1
#define ANSI_STATE_CSI     2

#define ANSI_MAX_PARAMS    8

static int ansi_state = ANSI_STATE_NORMAL;
static int ansi_params[ANSI_MAX_PARAMS];
static int ansi_param_count = 0;
static int ansi_current_param = 0;
static int ansi_param_started = 0;

static uint8_t default_fg = VGA_WHITE;
static uint8_t default_bg = VGA_BLACK;

static uint8_t ansi_to_vga_color(int ansi_code) {
    /* ANSI standard 30-37 / 40-47 → VGA color index */
    static const uint8_t map[8] = {
        VGA_BLACK, VGA_RED, VGA_GREEN, VGA_BROWN,
        VGA_BLUE, VGA_MAGENTA, VGA_CYAN, VGA_LIGHT_GREY
    };
    if (ansi_code >= 0 && ansi_code < 8) {
        return map[ansi_code];
    }
    return VGA_WHITE;
}

static uint8_t ansi_to_vga_bright(int ansi_code) {
    /* ANSI 90-97 / 100-107 → bright VGA colors */
    static const uint8_t map[8] = {
        VGA_DARK_GREY, VGA_LIGHT_RED, VGA_LIGHT_GREEN, VGA_YELLOW,
        VGA_LIGHT_BLUE, VGA_LIGHT_MAGENTA, VGA_LIGHT_CYAN, VGA_WHITE
    };
    if (ansi_code >= 0 && ansi_code < 8) {
        return map[ansi_code];
    }
    return VGA_WHITE;
}

static void ansi_reset_params(void) {
    int i;
    for (i = 0; i < ANSI_MAX_PARAMS; i++) {
        ansi_params[i] = 0;
    }
    ansi_param_count = 0;
    ansi_current_param = 0;
    ansi_param_started = 0;
}

static void ansi_finish_param(void) {
    if (ansi_param_count < ANSI_MAX_PARAMS) {
        ansi_params[ansi_param_count] = ansi_current_param;
        ansi_param_count++;
    }
    ansi_current_param = 0;
    ansi_param_started = 0;
}

static int ansi_get_param(int index, int def) {
    if (index < ansi_param_count && ansi_params[index] > 0) {
        return ansi_params[index];
    }
    return def;
}

static void ansi_execute_sgr(void) {
    int i;
    uint8_t fg = default_fg;
    uint8_t bg = default_bg;

    if (ansi_param_count == 0) {
        /* ESC[m = reset */
        current_color = fg | (bg << 4);
        return;
    }

    fg = current_color & 0x0F;
    bg = (current_color >> 4) & 0x0F;

    for (i = 0; i < ansi_param_count; i++) {
        int p = ansi_params[i];

        if (p == 0) {
            fg = default_fg;
            bg = default_bg;
        } else if (p == 1) {
            /* Bold = bright foreground */
            if (fg < 8) {
                fg += 8;
            }
        } else if (p >= 30 && p <= 37) {
            fg = ansi_to_vga_color(p - 30);
        } else if (p == 39) {
            fg = default_fg;
        } else if (p >= 40 && p <= 47) {
            bg = ansi_to_vga_color(p - 40);
        } else if (p == 49) {
            bg = default_bg;
        } else if (p >= 90 && p <= 97) {
            fg = ansi_to_vga_bright(p - 90);
        } else if (p >= 100 && p <= 107) {
            bg = ansi_to_vga_bright(p - 100);
        }
    }

    current_color = fg | (bg << 4);
}

static void ansi_execute_csi(char cmd) {
    int n, m;
    uint16_t row, col;
    uint16_t blank;
    uint16_t i;

    switch (cmd) {
        case 'A': /* Cursor Up */
            n = ansi_get_param(0, 1);
            row = cursor / VGA_WIDTH;
            col = cursor % VGA_WIDTH;
            if ((uint16_t)n > row) {
                row = 0;
            } else {
                row -= (uint16_t)n;
            }
            cursor = row * VGA_WIDTH + col;
            break;

        case 'B': /* Cursor Down */
            n = ansi_get_param(0, 1);
            row = cursor / VGA_WIDTH;
            col = cursor % VGA_WIDTH;
            row += (uint16_t)n;
            if (row >= VGA_HEIGHT) {
                row = VGA_HEIGHT - 1;
            }
            cursor = row * VGA_WIDTH + col;
            break;

        case 'C': /* Cursor Forward */
            n = ansi_get_param(0, 1);
            col = cursor % VGA_WIDTH;
            col += (uint16_t)n;
            if (col >= VGA_WIDTH) {
                col = VGA_WIDTH - 1;
            }
            cursor = (cursor / VGA_WIDTH) * VGA_WIDTH + col;
            break;

        case 'D': /* Cursor Back */
            n = ansi_get_param(0, 1);
            col = cursor % VGA_WIDTH;
            if ((uint16_t)n > col) {
                col = 0;
            } else {
                col -= (uint16_t)n;
            }
            cursor = (cursor / VGA_WIDTH) * VGA_WIDTH + col;
            break;

        case 'H': /* Cursor Position (row;col) */
        case 'f':
            row = (uint16_t)(ansi_get_param(0, 1) - 1);
            col = (uint16_t)(ansi_get_param(1, 1) - 1);
            if (row >= VGA_HEIGHT) {
                row = VGA_HEIGHT - 1;
            }
            if (col >= VGA_WIDTH) {
                col = VGA_WIDTH - 1;
            }
            cursor = row * VGA_WIDTH + col;
            break;

        case 'J': /* Erase in Display */
            n = ansi_get_param(0, 0);
            blank = (current_color << 8) | ' ';
            if (n == 0) {
                /* Clear from cursor to end */
                for (i = cursor; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                    vga[i] = blank;
                }
            } else if (n == 1) {
                /* Clear from start to cursor */
                for (i = 0; i <= cursor; i++) {
                    vga[i] = blank;
                }
            } else if (n == 2) {
                /* Clear entire screen */
                for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
                    vga[i] = blank;
                }
                cursor = 0;
            }
            break;

        case 'K': /* Erase in Line */
            n = ansi_get_param(0, 0);
            blank = (current_color << 8) | ' ';
            row = cursor / VGA_WIDTH;
            if (n == 0) {
                /* Clear from cursor to end of line */
                for (i = cursor; i < (row + 1) * VGA_WIDTH; i++) {
                    vga[i] = blank;
                }
            } else if (n == 1) {
                /* Clear from start of line to cursor */
                for (i = row * VGA_WIDTH; i <= cursor; i++) {
                    vga[i] = blank;
                }
            } else if (n == 2) {
                /* Clear entire line */
                for (i = row * VGA_WIDTH; i < (row + 1) * VGA_WIDTH; i++) {
                    vga[i] = blank;
                }
            }
            break;

        case 'm': /* SGR - Select Graphic Rendition */
            ansi_execute_sgr();
            break;

        default:
            break;
    }

    vga_update_hw_cursor();
}

void vga_putchar(char c) {
    if (ansi_state == ANSI_STATE_NORMAL) {
        if (c == '\x1B') {
            ansi_state = ANSI_STATE_ESC;
            return;
        }

        if (c == '\n') {
            cursor = ((cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
        } else if (c == '\b') {
            if (cursor > 0) {
                cursor--;
                vga[cursor] = (current_color << 8) | ' ';
            }
        } else if (c == '\r') {
            cursor = (cursor / VGA_WIDTH) * VGA_WIDTH;
        } else {
            vga[cursor++] = (current_color << 8) | (uint8_t)c;
        }

        if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
            vga_scroll_up_one();
            cursor -= VGA_WIDTH;
        }

        vga_update_hw_cursor();
        return;
    }

    if (ansi_state == ANSI_STATE_ESC) {
        if (c == '[') {
            ansi_state = ANSI_STATE_CSI;
            ansi_reset_params();
        } else {
            /* Unknown escape, ignore and reset */
            ansi_state = ANSI_STATE_NORMAL;
        }
        return;
    }

    if (ansi_state == ANSI_STATE_CSI) {
        if (c >= '0' && c <= '9') {
            ansi_current_param = ansi_current_param * 10 + (c - '0');
            ansi_param_started = 1;
            return;
        }

        if (c == ';') {
            ansi_finish_param();
            return;
        }

        /* Final command character */
        if (ansi_param_started || ansi_param_count > 0) {
            ansi_finish_param();
        }

        ansi_execute_csi(c);
        ansi_state = ANSI_STATE_NORMAL;
        return;
    }
}

void vga_print(const char* s) {
    for (int i = 0; s[i]; i++) {
        vga_putchar(s[i]);
    }
}

uint16_t vga_get_cursor(void) {
    return cursor;
}

void vga_set_cursor(uint16_t pos) {
    if (pos >= VGA_WIDTH * VGA_HEIGHT) {
        pos = VGA_WIDTH * VGA_HEIGHT - 1;
    }
    cursor = pos;
    vga_update_hw_cursor();
}

void vga_put_at(uint16_t pos, char c) {
    if (pos < VGA_WIDTH * VGA_HEIGHT) {
        vga[pos] = (current_color << 8) | (uint8_t)c;
    }
}

char vga_char_at(uint16_t pos) {
    if (pos >= VGA_WIDTH * VGA_HEIGHT) {
        return ' ';
    }
    return (char)(vga[pos] & 0xFF);
}

/* --- Scrollback view functions --- */

static void vga_save_live_screen(void) {
    uint16_t i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        live_screen[i] = vga[i];
    }
    live_cursor = cursor;
}

static void vga_restore_live_screen(void) {
    uint16_t i;
    for (i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = live_screen[i];
    }
    cursor = live_cursor;
    vga_update_hw_cursor();
}

static void vga_render_scrollback_view(void) {
    int line;
    int row;
    int sb_idx;
    int live_lines;
    uint16_t blank = (current_color << 8) | ' ';

    /* We show VGA_HEIGHT lines total.
     * The bottom (VGA_HEIGHT - scroll_offset) lines come from the saved live screen.
     * The top scroll_offset lines come from the scrollback buffer. */

    live_lines = VGA_HEIGHT - scroll_offset;

    /* Top part: from scrollback */
    for (row = 0; row < scroll_offset && row < VGA_HEIGHT; row++) {
        /* scrollback_head points to next write slot. 
         * Most recent saved line is at (scrollback_head - 1).
         * We want lines from offset scroll_offset..1 (1 = most recent saved). 
         * Line for this row: offset = scroll_offset - row lines back from live. */
        line = scroll_offset - row; /* how many lines back from live top */
        sb_idx = (scrollback_head - line + VGA_SCROLLBACK_LINES) % VGA_SCROLLBACK_LINES;
        for (int col = 0; col < VGA_WIDTH; col++) {
            vga[row * VGA_WIDTH + col] = scrollback[sb_idx][col];
        }
    }

    /* Bottom part: from live screen, shifted */
    for (row = scroll_offset; row < VGA_HEIGHT; row++) {
        int src_row = row - scroll_offset;
        for (int col = 0; col < VGA_WIDTH; col++) {
            vga[row * VGA_WIDTH + col] = live_screen[src_row * VGA_WIDTH + col];
        }
    }

    /* Hide cursor when scrolled back */
    vga_outb(VGA_CRTC_INDEX, 0x0A);
    vga_outb(VGA_CRTC_DATA, 0x20); /* bit 5 = cursor disable */
}

void vga_scroll_view_up(int lines) {
    int max_offset;

    if (lines <= 0) {
        return;
    }

    max_offset = scrollback_count;
    if (max_offset > VGA_SCROLLBACK_LINES) {
        max_offset = VGA_SCROLLBACK_LINES;
    }

    if (scroll_offset == 0) {
        /* Entering scrollback mode: save current screen */
        vga_save_live_screen();
    }

    scroll_offset += lines;
    if (scroll_offset > max_offset) {
        scroll_offset = max_offset;
    }

    if (scroll_offset > 0) {
        vga_render_scrollback_view();
    }
}

void vga_scroll_view_down(int lines) {
    if (lines <= 0 || scroll_offset == 0) {
        return;
    }

    scroll_offset -= lines;
    if (scroll_offset <= 0) {
        scroll_offset = 0;
        vga_restore_live_screen();
        /* Re-enable cursor */
        vga_outb(VGA_CRTC_INDEX, 0x0A);
        vga_outb(VGA_CRTC_DATA, 0x0E); /* cursor start scanline */
        vga_outb(VGA_CRTC_INDEX, 0x0B);
        vga_outb(VGA_CRTC_DATA, 0x0F); /* cursor end scanline */
        return;
    }

    vga_render_scrollback_view();
}

int vga_is_scrolled_back(void) {
    return scroll_offset > 0;
}
