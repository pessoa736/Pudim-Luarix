#ifndef KEVENT_H
#define KEVENT_H

#include "utypes.h"

/*
 * Kernel Event System
 *
 * Supports named events with a circular queue and subscriber registry.
 * Events are queued by producers (keyboard driver, timer, Lua) and
 * dispatched to Lua callbacks by kevent_dispatch().
 *
 * Built-in event types:
 *   "key_pressed"  — keyboard input (data = key code / string)
 *   "timer"        — periodic timer tick (configurable interval)
 *   "io"           — serial I/O activity
 *   Custom strings — user-defined from Lua via event.emit()
 */

#define KEVENT_MAX_NAME       32u
#define KEVENT_MAX_DATA      128u
#define KEVENT_QUEUE_SIZE     64u
#define KEVENT_MAX_SUBS       32u

/* Event data type tag */
typedef enum kevent_dtype {
    KEVENT_DATA_NONE = 0,
    KEVENT_DATA_STRING,
    KEVENT_DATA_INT,
    KEVENT_DATA_BOOL
} kevent_dtype_t;

/* A single event in the queue */
typedef struct kevent {
    char          name[KEVENT_MAX_NAME];
    kevent_dtype_t dtype;
    union {
        char     str[KEVENT_MAX_DATA];
        long     ival;
        int      bval;
    } data;
} kevent_t;

/* Subscriber entry: event name → Lua callback ref */
typedef struct kevent_sub {
    int  active;
    char name[KEVENT_MAX_NAME];
    int  lua_ref;           /* LUA_REGISTRYINDEX reference */
    int  id;                /* unique subscriber id */
} kevent_sub_t;

/* Initialize the event system */
void kevent_init(void);

/* Push an event onto the queue (producer side). Returns 1 on success. */
int kevent_push(const char* name, kevent_dtype_t dtype, const void* data);

/* Push helpers for common types */
int kevent_push_string(const char* name, const char* value);
int kevent_push_int(const char* name, long value);
int kevent_push_none(const char* name);

/* Pop an event from the queue (consumer side). Returns 1 if event available. */
int kevent_pop(kevent_t* out);

/* Get number of pending events */
uint32_t kevent_pending(void);

/* Register a subscriber. Returns subscriber ID (>0), or 0 on failure. */
int kevent_subscribe(const char* name, int lua_ref);

/* Unsubscribe by ID. Returns 1 on success. */
int kevent_unsubscribe(int sub_id);

/* Unsubscribe all listeners for a given event name */
void kevent_unsubscribe_all(const char* name);

/* Clear all events and subscribers */
void kevent_clear(void);

/* Get subscriber count for a given event (or all if name is NULL) */
uint32_t kevent_subscriber_count(const char* name);

/* Timer event configuration */
void kevent_set_timer_interval(uint32_t ticks);
uint32_t kevent_get_timer_interval(void);
void kevent_timer_tick(void);

/* Accessors for dispatch (used by keventlua) */
uint32_t kevent_get_sub_count(void);
const kevent_sub_t* kevent_get_sub(uint32_t index);

#endif
