#include "kdebug.h"
#include "kheap.h"
#include "ksys.h"

/* ============================================================
 * Pudding Layers — command history ring buffer
 *
 * Like layers of pudding stacking in a mold, each command
 * you type adds a new layer. You can peel back through
 * the layers with Up/Down arrows.
 * ============================================================ */

static char g_layers[KDEBUG_MAX_LAYERS][KDEBUG_LAYER_SIZE];
static unsigned int g_layer_write;  /* next write position (ring) */
static unsigned int g_layer_count;  /* total stored layers */

static size_t kdebug_slen(const char* s) {
    size_t n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static void kdebug_scopy(char* dst, const char* src, size_t cap) {
    size_t i = 0;

    if (!dst || !src || cap == 0) {
        return;
    }

    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

void kdebug_history_init(void) {
    unsigned int i;

    for (i = 0; i < KDEBUG_MAX_LAYERS; i++) {
        g_layers[i][0] = 0;
    }

    g_layer_write = 0;
    g_layer_count = 0;
}

void kdebug_history_push(const char* cmd) {
    if (!cmd || !cmd[0]) {
        return;
    }

    /* Skip if identical to the most recent layer */
    if (g_layer_count > 0) {
        unsigned int last;

        if (g_layer_write == 0) {
            last = KDEBUG_MAX_LAYERS - 1;
        } else {
            last = g_layer_write - 1;
        }

        if (kdebug_slen(g_layers[last]) > 0) {
            size_t li = 0;
            int same = 1;

            while (cmd[li] && g_layers[last][li]) {
                if (cmd[li] != g_layers[last][li]) {
                    same = 0;
                    break;
                }
                li++;
            }

            if (same && cmd[li] == g_layers[last][li]) {
                return;
            }
        }
    }

    kdebug_scopy(g_layers[g_layer_write], cmd, KDEBUG_LAYER_SIZE);
    g_layer_write = (g_layer_write + 1) % KDEBUG_MAX_LAYERS;

    if (g_layer_count < KDEBUG_MAX_LAYERS) {
        g_layer_count++;
    }
}

const char* kdebug_history_at(unsigned int index) {
    unsigned int pos;

    if (index >= g_layer_count) {
        return 0;
    }

    /* index 0 = most recent, 1 = second most recent, etc. */
    if (g_layer_write >= index + 1) {
        pos = g_layer_write - index - 1;
    } else {
        pos = KDEBUG_MAX_LAYERS - (index + 1 - g_layer_write);
    }

    return g_layers[pos];
}

unsigned int kdebug_history_count(void) {
    return g_layer_count;
}

/* ============================================================
 * Baking Timer — execution profiler
 *
 * Like timing a pudding in the oven: start the timer,
 * mark checkpoints ("caramel melted", "custard set"),
 * then read how long each step took.
 * ============================================================ */

static uint64_t g_bake_start_ms;
static int      g_bake_running;

typedef struct {
    char label[32];
    uint64_t ms;  /* time since bake start */
} kdebug_mark_t;

static kdebug_mark_t g_bake_marks[KDEBUG_MAX_MARKS];
static unsigned int  g_bake_mark_count;

void kdebug_timer_reset(void) {
    unsigned int i;

    g_bake_start_ms = 0;
    g_bake_running = 0;
    g_bake_mark_count = 0;

    for (i = 0; i < KDEBUG_MAX_MARKS; i++) {
        g_bake_marks[i].label[0] = 0;
        g_bake_marks[i].ms = 0;
    }
}

void kdebug_timer_start(void) {
    g_bake_start_ms = ksys_uptime_ms();
    g_bake_running = 1;
    g_bake_mark_count = 0;
}

uint64_t kdebug_timer_stop(void) {
    uint64_t elapsed;

    if (!g_bake_running) {
        return 0;
    }

    elapsed = ksys_uptime_ms() - g_bake_start_ms;
    g_bake_running = 0;
    return elapsed;
}

int kdebug_timer_mark(const char* label) {
    if (!g_bake_running) {
        return 0;
    }

    if (g_bake_mark_count >= KDEBUG_MAX_MARKS) {
        return 0;
    }

    if (!label) {
        label = "?";
    }

    kdebug_scopy(g_bake_marks[g_bake_mark_count].label, label, 32);
    g_bake_marks[g_bake_mark_count].ms = ksys_uptime_ms() - g_bake_start_ms;
    g_bake_mark_count++;
    return 1;
}

unsigned int kdebug_timer_mark_count(void) {
    return g_bake_mark_count;
}

const char* kdebug_timer_mark_label(unsigned int index) {
    if (index >= g_bake_mark_count) {
        return 0;
    }
    return g_bake_marks[index].label;
}

uint64_t kdebug_timer_mark_ms(unsigned int index) {
    if (index >= g_bake_mark_count) {
        return 0;
    }
    return g_bake_marks[index].ms;
}

/* ============================================================
 * Ingredient Check — heap inspector
 *
 * Like checking the ingredients before baking: how many
 * portions are prepared, which ones are free, and what's
 * the biggest chunk available.
 * ============================================================ */

unsigned int kdebug_heap_block_count(void) {
    return kheap_block_count();
}

unsigned int kdebug_heap_free_block_count(void) {
    return kheap_free_block_count();
}

size_t kdebug_heap_largest_free(void) {
    return kheap_largest_free_block();
}
