#include "ksyslua.h"
#include "ksys.h"

/* Lua: sys.version() */
static int ksyslua_version(lua_State* L) {
    const char* version = ksys_version();
    lua_pushstring(L, version);
    return 1;
}

/* Lua: sys.uptime_ms() */
static int ksyslua_uptime_ms(lua_State* L) {
    uint64_t uptime = ksys_uptime_ms();
    lua_pushinteger(L, (lua_Integer)uptime);
    return 1;
}

/* Lua: sys.uptime_us() */
static int ksyslua_uptime_us(lua_State* L) {
    uint64_t uptime = ksys_uptime_us();
    lua_pushinteger(L, (lua_Integer)uptime);
    return 1;
}

/* Lua: sys.memory_total() */
static int ksyslua_memory_total(lua_State* L) {
    uint64_t total = ksys_memory_total();
    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

/* Lua: sys.memory_free() */
static int ksyslua_memory_free(lua_State* L) {
    uint64_t free = ksys_memory_free();
    lua_pushinteger(L, (lua_Integer)free);
    return 1;
}

/* Lua: sys.memory_used() */
static int ksyslua_memory_used(lua_State* L) {
    uint64_t used = ksys_memory_used();
    lua_pushinteger(L, (lua_Integer)used);
    return 1;
}

/* Lua: sys.cpu_count() */
static int ksyslua_cpu_count(lua_State* L) {
    uint32_t count = ksys_cpu_count();
    lua_pushinteger(L, (lua_Integer)count);
    return 1;
}

int ksyslua_register(lua_State* L) {
    /* Create sys table */
    lua_createtable(L, 0, 7);
    
    lua_pushcfunction(L, ksyslua_version);
    lua_setfield(L, -2, "version");
    
    lua_pushcfunction(L, ksyslua_uptime_ms);
    lua_setfield(L, -2, "uptime_ms");
    
    lua_pushcfunction(L, ksyslua_uptime_us);
    lua_setfield(L, -2, "uptime_us");
    
    lua_pushcfunction(L, ksyslua_memory_total);
    lua_setfield(L, -2, "memory_total");
    
    lua_pushcfunction(L, ksyslua_memory_free);
    lua_setfield(L, -2, "memory_free");
    
    lua_pushcfunction(L, ksyslua_memory_used);
    lua_setfield(L, -2, "memory_used");
    
    lua_pushcfunction(L, ksyslua_cpu_count);
    lua_setfield(L, -2, "cpu_count");
    
    lua_setglobal(L, "sys");
    return 1;
}
