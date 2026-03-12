#include "kdebuglua.h"
#include "kdebug.h"
#include "kheap.h"
#include "kio.h"
#include "ksys.h"
#include "../lua/src/lauxlib.h"

/*
 * kdebuglua — Lua bindings for the debug/profiler/inspector module
 *
 * Pudding analogies in English:
 *   debug.locals()       — "toothpick test": poke into the pudding to
 *                          see if the inside is set (inspect local vars)
 *   debug.stack()        — check which layers are stacked in the mold
 *   debug.globals()      — look at all ingredients on the counter
 *   debug.timer_start()  — start the baking timer
 *   debug.timer_stop()   — ding! pudding is done, returns bake time
 *   debug.timer_mark()   — mark a checkpoint ("caramel melted")
 *   debug.timer_report() — read the full baking log
 *   debug.heap()         — ingredient check: inspect heap blocks
 *   debug.lua_mem()      — how much batter the Lua VM is using
 *   debug.fragmentation()— is the batter evenly mixed or clumpy?
 *   debug.history()      — list pudding layers (command history)
 */

/* --- Toothpick Test: Lua introspection --- */

/* debug.locals([level]) — inspect local variables at a call level */
static int kdebuglua_locals(lua_State* L) {
    int level = 1;
    lua_Debug ar;
    int idx;

    if (lua_gettop(L) >= 1 && lua_isinteger(L, 1)) {
        level = (int)lua_tointeger(L, 1);
        if (level < 0) {
            level = 1;
        }
    }

    /* level+1 to skip this C function frame */
    if (!lua_getstack(L, level, &ar)) {
        lua_pushstring(L, "(no stack at that level)");
        lua_pushnil(L);
        return 2;
    }

    lua_createtable(L, 0, 8);
    idx = 1;

    while (1) {
        const char* name = lua_getlocal(L, &ar, idx);
        if (!name) {
            break;
        }
        /* Stack: table, value */
        lua_setfield(L, -2, name);
        idx++;
    }

    return 1;
}

/* debug.stack() — show call stack (like checking stacked pudding layers) */
static int kdebuglua_stack(lua_State* L) {
    lua_Debug ar;
    int level;
    char buf[16];

    lua_pushstring(L, "stack traceback:");

    for (level = 1; level < 16; level++) {
        if (!lua_getstack(L, level, &ar)) {
            break;
        }

        lua_getinfo(L, "Snl", &ar);

        lua_pushstring(L, "\n  ");

        if (ar.source && ar.source[0]) {
            lua_pushstring(L, ar.source);
        } else {
            lua_pushstring(L, "?");
        }

        if (ar.currentline > 0) {
            /* Convert line number to string */
            int val = ar.currentline;
            int n = 0;
            int p;
            char tmp[16];

            if (val == 0) {
                buf[0] = '0';
                buf[1] = 0;
            } else {
                while (val > 0 && n < 15) {
                    tmp[n++] = (char)('0' + (val % 10));
                    val /= 10;
                }
                p = 0;
                while (n > 0) {
                    buf[p++] = tmp[--n];
                }
                buf[p] = 0;
            }

            lua_pushstring(L, ":");
            lua_pushstring(L, buf);
        }

        lua_pushstring(L, ": in ");

        if (ar.name && ar.name[0]) {
            lua_pushstring(L, "'");
            lua_pushstring(L, ar.name);
            lua_pushstring(L, "'");
        } else if (ar.what && ar.what[0]) {
            lua_pushstring(L, ar.what);
        } else {
            lua_pushstring(L, "?");
        }

        lua_concat(L, lua_gettop(L));
    }

    lua_concat(L, lua_gettop(L));
    return 1;
}

/* debug.globals() — list all global variable names (ingredients on counter) */
static int kdebuglua_globals(lua_State* L) {
    int count = 0;

    lua_createtable(L, 0, 16);
    lua_pushglobaltable(L);
    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char* key = lua_tostring(L, -2);
            const char* tname = luaL_typename(L, -1);

            lua_pushstring(L, tname);
            lua_setfield(L, -5, key); /* result_table[key] = typename */
            count++;
        }
        lua_pop(L, 1); /* pop value, keep key */
    }

    lua_pop(L, 1); /* pop _G */
    return 1;        /* return result table */
}

/* --- Baking Timer: profiler --- */

/* debug.timer_start() — preheat the oven */
static int kdebuglua_timer_start(lua_State* L) {
    (void)L;
    kdebug_timer_start();
    return 0;
}

/* debug.timer_stop() — ding! returns elapsed ms */
static int kdebuglua_timer_stop(lua_State* L) {
    uint64_t ms = kdebug_timer_stop();
    lua_pushinteger(L, (lua_Integer)ms);
    return 1;
}

/* debug.timer_mark(label) — mark a checkpoint in the bake */
static int kdebuglua_timer_mark(lua_State* L) {
    const char* label = "mark";

    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        label = lua_tostring(L, 1);
    }

    lua_pushboolean(L, kdebug_timer_mark(label));
    return 1;
}

/* debug.timer_reset() — throw out the batch, start fresh */
static int kdebuglua_timer_reset(lua_State* L) {
    (void)L;
    kdebug_timer_reset();
    return 0;
}

/* debug.timer_report() — returns table of {label, ms} checkpoint entries */
static int kdebuglua_timer_report(lua_State* L) {
    unsigned int count = kdebug_timer_mark_count();
    unsigned int i;

    lua_createtable(L, (int)count, 0);

    for (i = 0; i < count; i++) {
        lua_createtable(L, 0, 2);

        lua_pushstring(L, kdebug_timer_mark_label(i));
        lua_setfield(L, -2, "label");

        lua_pushinteger(L, (lua_Integer)kdebug_timer_mark_ms(i));
        lua_setfield(L, -2, "ms");

        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }

    return 1;
}

/* --- Ingredient Check: heap & memory inspection --- */

/* debug.heap() — returns table with heap block stats */
static int kdebuglua_heap(lua_State* L) {
    lua_createtable(L, 0, 5);

    lua_pushinteger(L, (lua_Integer)kdebug_heap_block_count());
    lua_setfield(L, -2, "blocks");

    lua_pushinteger(L, (lua_Integer)kdebug_heap_free_block_count());
    lua_setfield(L, -2, "free_blocks");

    lua_pushinteger(L, (lua_Integer)kdebug_heap_largest_free());
    lua_setfield(L, -2, "largest_free");

    lua_pushinteger(L, (lua_Integer)kheap_free_bytes());
    lua_setfield(L, -2, "free_bytes");

    lua_pushinteger(L, (lua_Integer)kheap_total_bytes());
    lua_setfield(L, -2, "total_bytes");

    return 1;
}

/* debug.lua_mem() — returns Lua VM memory usage in KB */
static int kdebuglua_lua_mem(lua_State* L) {
    int kb = lua_gc(L, LUA_GCCOUNT, 0);
    int bytes = lua_gc(L, LUA_GCCOUNTB, 0);

    lua_createtable(L, 0, 2);

    lua_pushinteger(L, kb);
    lua_setfield(L, -2, "kb");

    lua_pushinteger(L, bytes);
    lua_setfield(L, -2, "bytes_remainder");

    return 1;
}

/* debug.fragmentation() — returns ratio (0.0..1.0) of free blocks vs total */
static int kdebuglua_fragmentation(lua_State* L) {
    unsigned int total = kdebug_heap_block_count();
    unsigned int free_b = kdebug_heap_free_block_count();

    if (total == 0) {
        lua_pushnumber(L, 0.0);
    } else {
        lua_pushnumber(L, (lua_Number)free_b / (lua_Number)total);
    }

    return 1;
}

/* debug.history() — returns table of past commands (pudding layers) */
static int kdebuglua_history(lua_State* L) {
    unsigned int count = kdebug_history_count();
    unsigned int i;

    lua_createtable(L, (int)count, 0);

    for (i = 0; i < count; i++) {
        const char* entry = kdebug_history_at(i);
        if (entry) {
            lua_pushstring(L, entry);
        } else {
            lua_pushstring(L, "");
        }
        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }

    return 1;
}

/* --- Registration --- */

int kdebuglua_register(lua_State* L) {
    kdebug_history_init();
    kdebug_timer_reset();

    lua_createtable(L, 0, 14);

    /* Toothpick test — Lua introspection */
    lua_pushcfunction(L, kdebuglua_locals);
    lua_setfield(L, -2, "locals");

    lua_pushcfunction(L, kdebuglua_stack);
    lua_setfield(L, -2, "stack");

    lua_pushcfunction(L, kdebuglua_globals);
    lua_setfield(L, -2, "globals");

    /* Baking timer — profiler */
    lua_pushcfunction(L, kdebuglua_timer_start);
    lua_setfield(L, -2, "timer_start");

    lua_pushcfunction(L, kdebuglua_timer_stop);
    lua_setfield(L, -2, "timer_stop");

    lua_pushcfunction(L, kdebuglua_timer_mark);
    lua_setfield(L, -2, "timer_mark");

    lua_pushcfunction(L, kdebuglua_timer_reset);
    lua_setfield(L, -2, "timer_reset");

    lua_pushcfunction(L, kdebuglua_timer_report);
    lua_setfield(L, -2, "timer_report");

    /* Ingredient check — heap & memory */
    lua_pushcfunction(L, kdebuglua_heap);
    lua_setfield(L, -2, "heap");

    lua_pushcfunction(L, kdebuglua_lua_mem);
    lua_setfield(L, -2, "lua_mem");

    lua_pushcfunction(L, kdebuglua_fragmentation);
    lua_setfield(L, -2, "fragmentation");

    /* Pudding layers — history */
    lua_pushcfunction(L, kdebuglua_history);
    lua_setfield(L, -2, "history");

    lua_setglobal(L, "debug");
    return 1;
}
