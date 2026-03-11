#include "ksyslua.h"
#include "ksys.h"

static void ksyslua_push_hex_field(lua_State* L, const char* field, const char* value) {
    lua_pushstring(L, value ? value : "0x0");
    lua_setfield(L, -2, field);
}

static const char* ksyslua_memory_type_name(uint32_t type) {
    if (type == 1u) {
        return "usable";
    }
    if (type == 2u) {
        return "reserved";
    }
    if (type == 3u) {
        return "acpi_reclaim";
    }
    if (type == 4u) {
        return "acpi_nvs";
    }
    if (type == 5u) {
        return "bad";
    }
    return "unknown";
}

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

/* Lua: sys.heap_total() */
static int ksyslua_heap_total(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    uint64_t total = ksys_heap_total();
    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

/* Lua: sys.boot_media_total() */
static int ksyslua_boot_media_total(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    uint64_t total = ksys_boot_media_total();
    lua_pushinteger(L, (lua_Integer)total);
    return 1;
}

/* Lua: sys.rom_total() */
static int ksyslua_rom_total(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)ksys_rom_total());
    return 1;
}

static int ksyslua_kernel_image_size(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)ksys_kernel_image_size());
    return 1;
}

/* Lua: sys.cpu_count() */
static int ksyslua_cpu_count(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    uint32_t count = ksys_cpu_count();
    lua_pushinteger(L, (lua_Integer)count);
    return 1;
}

/* Lua: sys.cpu_physical_cores() */
static int ksyslua_cpu_physical_cores(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)ksys_cpu_physical_cores());
    return 1;
}

static int ksyslua_cpu_vendor(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushstring(L, "");
        return 1;
    }

    lua_pushstring(L, ksys_cpu_vendor());
    return 1;
}

static int ksyslua_cpu_brand(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushstring(L, "");
        return 1;
    }

    lua_pushstring(L, ksys_cpu_brand());
    return 1;
}

/* Lua: sys.memory_map_count() */
static int ksyslua_memory_map_count(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)ksys_memory_map_count());
    return 1;
}

/* Lua: sys.memory_map_entry(index) */
static int ksyslua_memory_map_entry(lua_State* L) {
    lua_Integer index;
    uint64_t base;
    uint64_t length;
    uint32_t type;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    index = lua_tointeger(L, 1);
    if (index <= 0) {
        lua_pushnil(L);
        return 1;
    }

    base = ksys_memory_map_base((uint32_t)(index - 1));
    length = ksys_memory_map_length((uint32_t)(index - 1));
    type = ksys_memory_map_type((uint32_t)(index - 1));
    if (length == 0 && type == 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 6);
    lua_pushinteger(L, (lua_Integer)base);
    lua_setfield(L, -2, "base");
    lua_pushinteger(L, (lua_Integer)length);
    lua_setfield(L, -2, "length");
    lua_pushinteger(L, (lua_Integer)type);
    lua_setfield(L, -2, "type");
    lua_pushstring(L, ksyslua_memory_type_name(type));
    lua_setfield(L, -2, "kind");
    ksyslua_push_hex_field(L, "base_hex", ksys_memory_map_base_hex((uint32_t)(index - 1)));
    ksyslua_push_hex_field(L, "length_hex", ksys_memory_map_length_hex((uint32_t)(index - 1)));
    return 1;
}

int ksyslua_register(lua_State* L) {
    /* Create sys table */
    lua_createtable(L, 0, 17);
    
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

    lua_pushcfunction(L, ksyslua_heap_total);
    lua_setfield(L, -2, "heap_total");

    lua_pushcfunction(L, ksyslua_boot_media_total);
    lua_setfield(L, -2, "boot_media_total");

    lua_pushcfunction(L, ksyslua_rom_total);
    lua_setfield(L, -2, "rom_total");

    lua_pushcfunction(L, ksyslua_kernel_image_size);
    lua_setfield(L, -2, "kernel_image_size");
    
    lua_pushcfunction(L, ksyslua_cpu_count);
    lua_setfield(L, -2, "cpu_count");

    lua_pushcfunction(L, ksyslua_cpu_physical_cores);
    lua_setfield(L, -2, "cpu_physical_cores");

    lua_pushcfunction(L, ksyslua_cpu_vendor);
    lua_setfield(L, -2, "cpu_vendor");

    lua_pushcfunction(L, ksyslua_cpu_brand);
    lua_setfield(L, -2, "cpu_brand");

    lua_pushcfunction(L, ksyslua_memory_map_count);
    lua_setfield(L, -2, "memory_map_count");

    lua_pushcfunction(L, ksyslua_memory_map_entry);
    lua_setfield(L, -2, "memory_map_entry");
    
    lua_setglobal(L, "sys");
    return 1;
}
