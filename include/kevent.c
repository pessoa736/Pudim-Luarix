#include "kevent.h"
#include "serial.h"

/* ---- Circular event queue ---- */

static kevent_t g_event_queue[KEVENT_QUEUE_SIZE];
static uint32_t g_queue_head = 0;  /* next write position */
static uint32_t g_queue_tail = 0;  /* next read position */
static uint32_t g_queue_count = 0;

/* ---- Subscriber table ---- */

static kevent_sub_t g_subs[KEVENT_MAX_SUBS];
static int g_next_sub_id = 1;

/* ---- Timer event state ---- */

static uint32_t g_timer_interval = 100;  /* emit "timer" every N ticks */
static uint32_t g_timer_counter = 0;

/* ---- Spinlock ---- */

static volatile uint32_t g_event_lock = 0;

static inline void kevent_lock(void) {
    while (1) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        uint32_t oldval;

        asm volatile(
            "lock cmpxchgl %2, %1"
            : "=a"(oldval), "+m"(g_event_lock)
            : "r"(desired), "0"(expected)
            : "cc"
        );

        if (oldval == expected) {
            return;
        }

        asm volatile("pause");
    }
}

static inline void kevent_unlock(void) {
    g_event_lock = 0;
}

/* ---- String helpers ---- */

static uint32_t kevent_strlen(const char* s) {
    uint32_t len = 0;

    if (!s) {
        return 0;
    }

    while (s[len]) {
        len++;
    }

    return len;
}

static void kevent_strncpy0(char* dst, const char* src, uint32_t cap) {
    uint32_t i = 0;

    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static int kevent_streq(const char* a, const char* b) {
    uint32_t i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == b[i];
}

/* ---- Public API ---- */

void kevent_init(void) {
    uint32_t i;

    kevent_lock();

    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    g_next_sub_id = 1;
    g_timer_interval = 100;
    g_timer_counter = 0;

    for (i = 0; i < KEVENT_QUEUE_SIZE; i++) {
        g_event_queue[i].name[0] = 0;
        g_event_queue[i].dtype = KEVENT_DATA_NONE;
    }

    for (i = 0; i < KEVENT_MAX_SUBS; i++) {
        g_subs[i].active = 0;
        g_subs[i].name[0] = 0;
        g_subs[i].lua_ref = 0;
        g_subs[i].id = 0;
    }

    kevent_unlock();
}

int kevent_push(const char* name, kevent_dtype_t dtype, const void* data) {
    uint32_t nlen;
    kevent_t* ev;

    if (!name) {
        return 0;
    }

    nlen = kevent_strlen(name);

    if (nlen == 0 || nlen >= KEVENT_MAX_NAME) {
        return 0;
    }

    kevent_lock();

    if (g_queue_count >= KEVENT_QUEUE_SIZE) {
        /* Queue full: drop oldest event */
        g_queue_tail = (g_queue_tail + 1) % KEVENT_QUEUE_SIZE;
        g_queue_count--;
    }

    ev = &g_event_queue[g_queue_head];
    kevent_strncpy0(ev->name, name, KEVENT_MAX_NAME);
    ev->dtype = dtype;

    switch (dtype) {
        case KEVENT_DATA_STRING:
            if (data) {
                kevent_strncpy0(ev->data.str, (const char*)data, KEVENT_MAX_DATA);
            } else {
                ev->data.str[0] = 0;
            }
            break;

        case KEVENT_DATA_INT:
            if (data) {
                ev->data.ival = *(const int64_t*)data;
            } else {
                ev->data.ival = 0;
            }
            break;

        case KEVENT_DATA_BOOL:
            if (data) {
                ev->data.bval = *(const int*)data;
            } else {
                ev->data.bval = 0;
            }
            break;

        default:
            ev->data.ival = 0;
            break;
    }

    g_queue_head = (g_queue_head + 1) % KEVENT_QUEUE_SIZE;
    g_queue_count++;

    kevent_unlock();
    return 1;
}

int kevent_push_string(const char* name, const char* value) {
    return kevent_push(name, KEVENT_DATA_STRING, value);
}

int kevent_push_int(const char* name, long value) {
    return kevent_push(name, KEVENT_DATA_INT, &value);
}

int kevent_push_none(const char* name) {
    return kevent_push(name, KEVENT_DATA_NONE, (void*)0);
}

int kevent_pop(kevent_t* out) {
    kevent_t* ev;

    if (!out) {
        return 0;
    }

    kevent_lock();

    if (g_queue_count == 0) {
        kevent_unlock();
        return 0;
    }

    ev = &g_event_queue[g_queue_tail];

    kevent_strncpy0(out->name, ev->name, KEVENT_MAX_NAME);
    out->dtype = ev->dtype;

    switch (ev->dtype) {
        case KEVENT_DATA_STRING:
            kevent_strncpy0(out->data.str, ev->data.str, KEVENT_MAX_DATA);
            break;
        case KEVENT_DATA_INT:
            out->data.ival = ev->data.ival;
            break;
        case KEVENT_DATA_BOOL:
            out->data.bval = ev->data.bval;
            break;
        default:
            out->data.ival = 0;
            break;
    }

    g_queue_tail = (g_queue_tail + 1) % KEVENT_QUEUE_SIZE;
    g_queue_count--;

    kevent_unlock();
    return 1;
}

uint32_t kevent_pending(void) {
    return g_queue_count;
}

int kevent_subscribe(const char* name, int lua_ref) {
    uint32_t i;
    int sid;

    if (!name || kevent_strlen(name) == 0 || kevent_strlen(name) >= KEVENT_MAX_NAME) {
        return 0;
    }

    kevent_lock();

    for (i = 0; i < KEVENT_MAX_SUBS; i++) {
        if (!g_subs[i].active) {
            sid = g_next_sub_id++;
            g_subs[i].active = 1;
            kevent_strncpy0(g_subs[i].name, name, KEVENT_MAX_NAME);
            g_subs[i].lua_ref = lua_ref;
            g_subs[i].id = sid;

            kevent_unlock();
            return sid;
        }
    }

    kevent_unlock();
    return 0;
}

int kevent_unsubscribe(int sub_id) {
    uint32_t i;

    if (sub_id <= 0) {
        return 0;
    }

    kevent_lock();

    for (i = 0; i < KEVENT_MAX_SUBS; i++) {
        if (g_subs[i].active && g_subs[i].id == sub_id) {
            g_subs[i].active = 0;
            g_subs[i].name[0] = 0;
            g_subs[i].lua_ref = 0;
            g_subs[i].id = 0;

            kevent_unlock();
            return 1;
        }
    }

    kevent_unlock();
    return 0;
}

void kevent_unsubscribe_all(const char* name) {
    uint32_t i;

    if (!name) {
        return;
    }

    kevent_lock();

    for (i = 0; i < KEVENT_MAX_SUBS; i++) {
        if (g_subs[i].active && kevent_streq(g_subs[i].name, name)) {
            g_subs[i].active = 0;
            g_subs[i].name[0] = 0;
            g_subs[i].lua_ref = 0;
            g_subs[i].id = 0;
        }
    }

    kevent_unlock();
}

void kevent_clear(void) {
    kevent_init();
}

uint32_t kevent_subscriber_count(const char* name) {
    uint32_t i;
    uint32_t count = 0;

    for (i = 0; i < KEVENT_MAX_SUBS; i++) {
        if (!g_subs[i].active) {
            continue;
        }

        if (!name || kevent_streq(g_subs[i].name, name)) {
            count++;
        }
    }

    return count;
}

/* ---- Timer integration ---- */

void kevent_set_timer_interval(uint32_t ticks) {
    if (ticks == 0) {
        ticks = 1;
    }

    g_timer_interval = ticks;
    g_timer_counter = 0;
}

uint32_t kevent_get_timer_interval(void) {
    return g_timer_interval;
}

void kevent_timer_tick(void) {
    g_timer_counter++;

    if (g_timer_counter >= g_timer_interval) {
        g_timer_counter = 0;

        /* Only push timer event if someone is subscribed */
        if (kevent_subscriber_count("timer") > 0) {
            kevent_push_none("timer");
        }
    }
}

/* ---- Accessor for dispatch (used by keventlua) ---- */

uint32_t kevent_get_sub_count(void) {
    return KEVENT_MAX_SUBS;
}

const kevent_sub_t* kevent_get_sub(uint32_t index) {
    if (index >= KEVENT_MAX_SUBS) {
        return (void*)0;
    }

    return &g_subs[index];
}
