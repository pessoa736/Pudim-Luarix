/* Unit tests for kevent.c */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/* Stub serial */
void serial_print(const char* s) { (void)s; }

/* Include kevent source directly */
#include "../include/kevent.c"

static void test_init(void) {
    TEST_SUITE("kevent: init");
    kevent_init();

    ASSERT_EQ(kevent_pending(), 0, "no pending events after init");
    ASSERT_EQ(kevent_subscriber_count(NULL), 0, "no subscribers after init");
}

static void test_push_pop(void) {
    kevent_t ev;
    int r;

    TEST_SUITE("kevent: push and pop");
    kevent_init();

    r = kevent_push_string("key_pressed", "a");
    ASSERT_EQ(r, 1, "push returns 1");
    ASSERT_EQ(kevent_pending(), 1, "1 pending");

    r = kevent_pop(&ev);
    ASSERT_EQ(r, 1, "pop returns 1");
    ASSERT_STR_EQ(ev.name, "key_pressed", "event name matches");
    ASSERT_EQ(ev.dtype, KEVENT_DATA_STRING, "dtype is string");
    ASSERT_STR_EQ(ev.data.str, "a", "data matches");
    ASSERT_EQ(kevent_pending(), 0, "0 pending after pop");
}

static void test_push_int(void) {
    kevent_t ev;

    TEST_SUITE("kevent: push int");
    kevent_init();

    kevent_push_int("score", 42);
    kevent_pop(&ev);

    ASSERT_STR_EQ(ev.name, "score", "name");
    ASSERT_EQ(ev.dtype, KEVENT_DATA_INT, "dtype is int");
    ASSERT_EQ(ev.data.ival, 42, "int value");
}

static void test_push_none(void) {
    kevent_t ev;

    TEST_SUITE("kevent: push none");
    kevent_init();

    kevent_push_none("tick");
    kevent_pop(&ev);

    ASSERT_STR_EQ(ev.name, "tick", "name");
    ASSERT_EQ(ev.dtype, KEVENT_DATA_NONE, "dtype is none");
}

static void test_fifo_order(void) {
    kevent_t ev;

    TEST_SUITE("kevent: FIFO order");
    kevent_init();

    kevent_push_string("ev", "first");
    kevent_push_string("ev", "second");
    kevent_push_string("ev", "third");

    kevent_pop(&ev);
    ASSERT_STR_EQ(ev.data.str, "first", "first out");
    kevent_pop(&ev);
    ASSERT_STR_EQ(ev.data.str, "second", "second out");
    kevent_pop(&ev);
    ASSERT_STR_EQ(ev.data.str, "third", "third out");
}

static void test_queue_overflow(void) {
    kevent_t ev;
    uint32_t i;

    TEST_SUITE("kevent: queue overflow drops oldest");
    kevent_init();

    /* Fill queue completely */
    for (i = 0; i < KEVENT_QUEUE_SIZE; i++) {
        kevent_push_none("fill");
    }
    ASSERT_EQ(kevent_pending(), KEVENT_QUEUE_SIZE, "queue full");

    /* Push one more - should drop oldest */
    kevent_push_string("ev", "overflow");
    ASSERT_EQ(kevent_pending(), KEVENT_QUEUE_SIZE, "still full");

    /* Drain all but last */
    for (i = 0; i < KEVENT_QUEUE_SIZE - 1; i++) {
        kevent_pop(&ev);
    }

    /* Last one should be our overflow push */
    kevent_pop(&ev);
    ASSERT_STR_EQ(ev.data.str, "overflow", "overflow event survived");
}

static void test_pop_empty(void) {
    kevent_t ev;

    TEST_SUITE("kevent: pop from empty queue");
    kevent_init();

    ASSERT_EQ(kevent_pop(&ev), 0, "pop empty returns 0");
    ASSERT_EQ(kevent_pop(NULL), 0, "pop null returns 0");
}

static void test_subscribe_unsubscribe(void) {
    int id;

    TEST_SUITE("kevent: subscribe and unsubscribe");
    kevent_init();

    id = kevent_subscribe("key_pressed", 10);
    ASSERT(id > 0, "subscribe returns positive id");
    ASSERT_EQ(kevent_subscriber_count("key_pressed"), 1, "1 subscriber");

    ASSERT_EQ(kevent_unsubscribe(id), 1, "unsubscribe returns 1");
    ASSERT_EQ(kevent_subscriber_count("key_pressed"), 0, "0 subscribers");
}

static void test_unsubscribe_all(void) {
    TEST_SUITE("kevent: unsubscribe_all");
    kevent_init();

    kevent_subscribe("ev", 1);
    kevent_subscribe("ev", 2);
    kevent_subscribe("other", 3);
    ASSERT_EQ(kevent_subscriber_count("ev"), 2, "2 ev subs");

    kevent_unsubscribe_all("ev");
    ASSERT_EQ(kevent_subscriber_count("ev"), 0, "0 ev subs");
    ASSERT_EQ(kevent_subscriber_count("other"), 1, "other sub intact");
}

static void test_timer_tick(void) {
    TEST_SUITE("kevent: timer tick");
    kevent_init();

    /* No subscriber for timer, so no events should be pushed */
    kevent_set_timer_interval(5);
    for (int i = 0; i < 5; i++) kevent_timer_tick();
    ASSERT_EQ(kevent_pending(), 0, "no timer event without sub");

    /* Add subscriber */
    kevent_subscribe("timer", 99);
    for (int i = 0; i < 5; i++) kevent_timer_tick();
    ASSERT_EQ(kevent_pending(), 1, "timer event emitted");
}

static void test_invalid_push(void) {
    TEST_SUITE("kevent: invalid push");
    kevent_init();

    ASSERT_EQ(kevent_push(NULL, KEVENT_DATA_NONE, NULL), 0, "null name");
    ASSERT_EQ(kevent_push("", KEVENT_DATA_NONE, NULL), 0, "empty name");
}

static void test_subscriber_accessor(void) {
    const kevent_sub_t* sub;

    TEST_SUITE("kevent: subscriber accessor");
    kevent_init();

    kevent_subscribe("test", 77);

    ASSERT_EQ(kevent_get_sub_count(), KEVENT_MAX_SUBS, "sub count is max");
    sub = kevent_get_sub(0);
    ASSERT_NOT_NULL(sub, "sub[0] not null");
    ASSERT_EQ(sub->active, 1, "sub is active");
    ASSERT_STR_EQ(sub->name, "test", "sub name");
    ASSERT_EQ(sub->lua_ref, 77, "lua_ref");
    ASSERT_NULL(kevent_get_sub(KEVENT_MAX_SUBS), "out of range returns null");
}

int main(void) {
    test_init();
    test_push_pop();
    test_push_int();
    test_push_none();
    test_fifo_order();
    test_queue_overflow();
    test_pop_empty();
    test_subscribe_unsubscribe();
    test_unsubscribe_all();
    test_timer_tick();
    test_invalid_push();
    test_subscriber_accessor();
    TEST_RESULTS();
}
