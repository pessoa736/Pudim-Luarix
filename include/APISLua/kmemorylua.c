#include "kmemorylua.h"
#include "kmemory.h"

#define KMEMORYLUA_STATS_BUF 128

static int kmemorylua_allocate(lua_State* L) {
    lua_Integer size;
    void* ptr;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    size = lua_tointeger(L, 1);
    if (size <= 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    ptr = kmemory_allocate((uint64_t)size);
    lua_pushinteger(L, (lua_Integer)(uint64_t)(size_t)ptr);
    return 1;
}

static int kmemorylua_free(lua_State* L) {
    lua_Integer ptr;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ptr = lua_tointeger(L, 1);
    if (ptr == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kmemory_free((void*)(size_t)ptr));
    return 1;
}

static int kmemorylua_protect(lua_State* L) {
    lua_Integer ptr;
    lua_Integer flags;

    if (lua_gettop(L) != 2 || !lua_isinteger(L, 1) || !lua_isinteger(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ptr = lua_tointeger(L, 1);
    flags = lua_tointeger(L, 2);

    if (ptr == 0 || flags < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kmemory_protect((void*)(size_t)ptr, (uint32_t)flags));
    return 1;
}

static int kmemorylua_dump_stats(lua_State* L) {
    char out[KMEMORYLUA_STATS_BUF];
    (void)L;

    kmemory_dump_stats(out, sizeof(out));
    lua_pushstring(L, out);
    return 1;
}

int kmemorylua_register(lua_State* L) {
    kmemory_init();

    lua_createtable(L, 0, 4);

    lua_pushcfunction(L, kmemorylua_allocate);
    lua_setfield(L, -2, "allocate");

    lua_pushcfunction(L, kmemorylua_free);
    lua_setfield(L, -2, "free");

    lua_pushcfunction(L, kmemorylua_protect);
    lua_setfield(L, -2, "protect");

    lua_pushcfunction(L, kmemorylua_dump_stats);
    lua_setfield(L, -2, "dump_stats");

    lua_setglobal(L, "memory");
    return 1;
}
