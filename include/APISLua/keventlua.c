#include "keventlua.h"
#include "kevent.h"
#include "serial.h"

#include "../lua/src/lua.h"

/*
 * Manual ref/unref using the Lua registry (avoids lauxlib dependency).
 * We use a simple incrementing counter as reference key.
 */
static int keventlua_next_ref = 1;

static int keventlua_ref(lua_State* L) {
    int ref;

    /* Value to store is at top of stack */
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return -1;  /* LUA_REFNIL equivalent */
    }

    ref = keventlua_next_ref++;

    /* registry[ref] = value (value is at top of stack) */
    lua_rawseti(L, LUA_REGISTRYINDEX, (lua_Integer)ref);

    return ref;
}

static void keventlua_unref(lua_State* L, int ref) {
    if (ref <= 0) {
        return;
    }

    /* registry[ref] = nil */
    lua_pushnil(L);
    lua_rawseti(L, LUA_REGISTRYINDEX, (lua_Integer)ref);
}

/*
 * Lua event API:
 *
 *   event.subscribe(name, callback)  → subscription_id
 *   event.unsubscribe(id)            → boolean
 *   event.emit(name [, data])        → boolean
 *   event.pending()                  → integer
 *   event.poll()                     → name, data | nil
 *   event.clear()                    → nil
 *   event.subscribers([name])        → integer
 *   event.timer_interval([ticks])    → integer (get/set)
 */

/* event.subscribe(name, callback) → id */
static int keventlua_subscribe(lua_State* L) {
    const char* name;
    int ref;
    int sid;

    if (!lua_isstring(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (!lua_isfunction(L, 2)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    name = lua_tostring(L, 1);

    /* Create a reference to the callback function */
    lua_pushvalue(L, 2);
    ref = keventlua_ref(L);

    if (ref <= 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    sid = kevent_subscribe(name, ref);

    if (sid == 0) {
        keventlua_unref(L, ref);
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)sid);
    return 1;
}

/* event.unsubscribe(id) → boolean */
static int keventlua_unsubscribe(lua_State* L) {
    int sid;

    if (!lua_isinteger(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    sid = (int)lua_tointeger(L, 1);

    /* Find the subscriber to release its Lua reference */
    {
        uint32_t i;
        uint32_t count = kevent_get_sub_count();

        for (i = 0; i < count; i++) {
            const kevent_sub_t* sub = kevent_get_sub(i);

            if (sub && sub->active && sub->id == sid) {
                keventlua_unref(L, sub->lua_ref);
                break;
            }
        }
    }

    lua_pushboolean(L, kevent_unsubscribe(sid));
    return 1;
}

/* event.emit(name [, data]) → boolean */
static int keventlua_emit(lua_State* L) {
    const char* name;
    int result;

    if (!lua_isstring(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    name = lua_tostring(L, 1);

    if (lua_gettop(L) < 2 || lua_isnil(L, 2)) {
        result = kevent_push_none(name);
    } else if (lua_isstring(L, 2)) {
        result = kevent_push_string(name, lua_tostring(L, 2));
    } else if (lua_isinteger(L, 2)) {
        result = kevent_push_int(name, (long)lua_tointeger(L, 2));
    } else if (lua_isnumber(L, 2)) {
        long ival = (long)lua_tonumber(L, 2);
        result = kevent_push_int(name, ival);
    } else if (lua_isboolean(L, 2)) {
        int bval = lua_toboolean(L, 2);
        result = kevent_push(name, KEVENT_DATA_BOOL, &bval);
    } else {
        result = kevent_push_none(name);
    }

    lua_pushboolean(L, result);
    return 1;
}

/* event.pending() → integer */
static int keventlua_pending(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)kevent_pending());
    return 1;
}

/* event.poll() → name, data | nil */
static int keventlua_poll(lua_State* L) {
    kevent_t ev;

    if (!kevent_pop(&ev)) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, ev.name);

    switch (ev.dtype) {
        case KEVENT_DATA_STRING:
            lua_pushstring(L, ev.data.str);
            break;
        case KEVENT_DATA_INT:
            lua_pushinteger(L, (lua_Integer)ev.data.ival);
            break;
        case KEVENT_DATA_BOOL:
            lua_pushboolean(L, ev.data.bval);
            break;
        default:
            lua_pushnil(L);
            break;
    }

    return 2;
}

/* event.clear() */
static int keventlua_clear(lua_State* L) {
    /* Release all Lua references first */
    uint32_t i;
    uint32_t count = kevent_get_sub_count();

    for (i = 0; i < count; i++) {
        const kevent_sub_t* sub = kevent_get_sub(i);

        if (sub && sub->active && sub->lua_ref > 0) {
            keventlua_unref(L, sub->lua_ref);
        }
    }

    kevent_clear();
    return 0;
}

/* event.subscribers([name]) → integer */
static int keventlua_subscribers(lua_State* L) {
    const char* name = NULL;

    if (lua_isstring(L, 1)) {
        name = lua_tostring(L, 1);
    }

    lua_pushinteger(L, (lua_Integer)kevent_subscriber_count(name));
    return 1;
}

/* event.timer_interval([ticks]) → integer */
static int keventlua_timer_interval(lua_State* L) {
    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        lua_Integer val = lua_tointeger(L, 1);

        if (val > 0 && val <= 100000) {
            kevent_set_timer_interval((uint32_t)val);
        }
    }

    lua_pushinteger(L, (lua_Integer)kevent_get_timer_interval());
    return 1;
}

/* ---- Dispatch: call Lua callbacks for pending events ---- */

void keventlua_dispatch(lua_State* L) {
    kevent_t ev;
    uint32_t dispatched = 0;
    uint32_t max_dispatch = 16;  /* limit per call to avoid stalling */

    if (!L) {
        return;
    }

    while (dispatched < max_dispatch && kevent_pop(&ev)) {
        uint32_t i;
        uint32_t count = kevent_get_sub_count();

        for (i = 0; i < count; i++) {
            const kevent_sub_t* sub = kevent_get_sub(i);
            uint32_t ni;
            int match;

            if (!sub || !sub->active) {
                continue;
            }

            /* Check name match */
            match = 1;
            ni = 0;

            while (sub->name[ni] || ev.name[ni]) {
                if (sub->name[ni] != ev.name[ni]) {
                    match = 0;
                    break;
                }
                ni++;
            }

            if (!match) {
                continue;
            }

            /* Call the Lua callback: callback(data) */
            lua_rawgeti(L, LUA_REGISTRYINDEX, sub->lua_ref);

            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                continue;
            }

            /* Push event data as argument */
            switch (ev.dtype) {
                case KEVENT_DATA_STRING:
                    lua_pushstring(L, ev.data.str);
                    break;
                case KEVENT_DATA_INT:
                    lua_pushinteger(L, (lua_Integer)ev.data.ival);
                    break;
                case KEVENT_DATA_BOOL:
                    lua_pushboolean(L, ev.data.bval);
                    break;
                default:
                    lua_pushnil(L);
                    break;
            }

            /* Protected call: callback(data) */
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);

                serial_print("[event] callback error: ");

                if (err) {
                    serial_print(err);
                }

                serial_print("\r\n");
                lua_pop(L, 1);
            }
        }

        dispatched++;
    }
}

/* ---- Registration ---- */

int keventlua_register(lua_State* L) {
    if (!L) {
        return 0;
    }

    lua_createtable(L, 0, 8);

    lua_pushcfunction(L, keventlua_subscribe);
    lua_setfield(L, -2, "subscribe");

    lua_pushcfunction(L, keventlua_unsubscribe);
    lua_setfield(L, -2, "unsubscribe");

    lua_pushcfunction(L, keventlua_emit);
    lua_setfield(L, -2, "emit");

    lua_pushcfunction(L, keventlua_pending);
    lua_setfield(L, -2, "pending");

    lua_pushcfunction(L, keventlua_poll);
    lua_setfield(L, -2, "poll");

    lua_pushcfunction(L, keventlua_clear);
    lua_setfield(L, -2, "clear");

    lua_pushcfunction(L, keventlua_subscribers);
    lua_setfield(L, -2, "subscribers");

    lua_pushcfunction(L, keventlua_timer_interval);
    lua_setfield(L, -2, "timer_interval");

    lua_setglobal(L, "event");

    return 1;
}
