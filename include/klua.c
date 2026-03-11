#include "klua.h"
#include "kheap.h"
#include "kio.h"
#include "kfslua.h"
#include "ksyslua.h"
#include "ksynclua.h"
#include "serial.h"
#include "vga.h"

#include "../lua/src/lua.h"

static lua_State* g_lua = NULL;

static void klua_copy_string(char* dst, unsigned int dst_size, const char* src) {
    unsigned int i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && (i + 1) < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static void* klua_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud;
    (void)osize;

    if (nsize == 0) {
        kfree(ptr);
        return NULL;
    }

    if (!ptr) {
        return kmalloc(nsize);
    }

    return krealloc(ptr, nsize);
}

static int klua_panic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print("[lua panic] ");

    if (msg) {
        vga_print(msg);
    }

    vga_print("\n");

    while (1) {
        asm volatile ("hlt");
    }
}

/* Helper to convert number to string */
static void klua_number_to_string(lua_Number num, char* buf, int bufsize) {
    if (bufsize < 2) return;
    
    int pos = 0;
    int is_negative = 0;
    
    /* Handle negative */
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    /* Check if integer */
    lua_Integer ival = (lua_Integer)num;
    if (num == ival && ival >= 0) {
        /* Integer conversion */
        if (is_negative && pos < bufsize - 1) {
            buf[pos++] = '-';
        }
        
        if (ival == 0) {
            if (pos < bufsize - 1) buf[pos++] = '0';
        } else {
            char temp[32];
            int tpos = 0;
            lua_Integer tmp = ival;
            while (tmp > 0 && tpos < 30) {
                temp[tpos++] = '0' + (tmp % 10);
                tmp /= 10;
            }
            for (int j = tpos - 1; j >= 0 && pos < bufsize - 1; j--) {
                buf[pos++] = temp[j];
            }
        }
    } else {
        /* Fallback for floats or out-of-range */
        if (pos < bufsize - 1) buf[pos++] = '?';
    }
    
    buf[pos] = 0;
}

static void klua_write_value(lua_State* L, int idx) {
    int type = lua_type(L, idx);

    if (type == LUA_TSTRING) {
        const char* str = lua_tostring(L, idx);
        if (str) {
            kio_write(str);
        }
    } else if (type == LUA_TNUMBER) {
        lua_Number num = lua_tonumber(L, idx);
        char buf[64];
        klua_number_to_string(num, buf, sizeof(buf));
        kio_write(buf);
    } else if (type == LUA_TBOOLEAN) {
        kio_write(lua_toboolean(L, idx) ? "true" : "false");
    } else if (type == LUA_TNIL) {
        kio_write("nil");
    } else {
        kio_write("[");
        kio_write(lua_typename(L, type));
        kio_write("]");
    }
}

/* Lua print() function - outputs to VGA */
static int klua_print(lua_State* L) {
    int nargs = lua_gettop(L);
    
    for (int i = 1; i <= nargs; i++) {
        if (i > 1) {
            kio_write("  ");
        }

        klua_write_value(L, i);
    }

    kio_write("\n");
    return 0;
}

static int klua_io_write(lua_State* L) {
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++) {
        klua_write_value(L, i);
    }

    return 0;
}

static int klua_io_writeln(lua_State* L) {
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++) {
        klua_write_value(L, i);
    }

    kio_write("\n");
    return 0;
}

static int klua_io_clear(lua_State* L) {
    (void)L;
    kio_clear();
    return 0;
}

static int klua_serial_write(lua_State* L) {
    int nargs = lua_gettop(L);
    int i;

    for (i = 1; i <= nargs; i++) {
        if (lua_isstring(L, i)) {
            serial_print(lua_tostring(L, i));
        }
    }

    return 0;
}

static int klua_serial_writeln(lua_State* L) {
    int nargs = lua_gettop(L);
    int i;

    for (i = 1; i <= nargs; i++) {
        if (lua_isstring(L, i)) {
            serial_print(lua_tostring(L, i));
        }
    }

    serial_print("\r\n");
    return 0;
}

int klua_init(void) {
    g_lua = lua_newstate(klua_alloc, NULL, 0x50554449u);

    if (!g_lua) {
        return 0;
    }

    lua_atpanic(g_lua, klua_panic);

    /* Register print function */
    lua_pushcfunction(g_lua, klua_print);
    lua_setglobal(g_lua, "print");

    /* Register io library */
    lua_createtable(g_lua, 0, 3);
    lua_pushcfunction(g_lua, klua_io_write);
    lua_setfield(g_lua, -2, "write");
    lua_pushcfunction(g_lua, klua_io_writeln);
    lua_setfield(g_lua, -2, "writeln");
    lua_pushcfunction(g_lua, klua_io_clear);
    lua_setfield(g_lua, -2, "clear");
    lua_setglobal(g_lua, "io");

    if (!kfslua_register(g_lua)) {
        return 0;
    }

    /* Register serial library */
    lua_createtable(g_lua, 0, 2);
    lua_pushcfunction(g_lua, klua_serial_write);
    lua_setfield(g_lua, -2, "write");
    lua_pushcfunction(g_lua, klua_serial_writeln);
    lua_setfield(g_lua, -2, "writeln");
    lua_setglobal(g_lua, "serial");

    if (!ksyslua_register(g_lua)) {
        return 0;
    }

    if (!ksynclua_register(g_lua)) {
        return 0;
    }

    return 1;
}

/* Reader function for lua_load: reads from a string buffer */
struct klua_reader_state {
    const char* code;
    size_t size;
    int finished;
};

static const char* klua_reader(lua_State* L, void* data, size_t* size) {
    (void)L;
    struct klua_reader_state* state = (struct klua_reader_state*)data;

    if (state->finished) {
        *size = 0;
        return NULL;
    }

    *size = state->size;
    state->finished = 1;
    return state->code;
}

int klua_run(const char* code) {
    if (!g_lua) {
        vga_print("[lua] VM not initialized\n");
        return 0;
    }

    if (!code) {
        vga_print("[lua] No code to execute\n");
        return 0;
    }

    /* Setup reader state */
    struct klua_reader_state reader_state;
    reader_state.code = code;
    reader_state.size = 0;
    
    /* Calculate code length */
    const char* p = code;
    while (*p) p++;
    reader_state.size = p - code;
    
    reader_state.finished = 0;

    /* Load the chunk */
    int load_result = lua_load(g_lua, klua_reader, &reader_state, "kernel_code", "t");
    if (load_result != LUA_OK) {
        vga_print("[lua] Load error: ");
        const char* err = lua_tostring(g_lua, -1);
        if (err) vga_print(err);
        vga_print("\n");
        lua_pop(g_lua, 1);
        return 0;
    }

    /* Execute the loaded function */
    int call_result = lua_pcall(g_lua, 0, LUA_MULTRET, 0);
    if (call_result != LUA_OK) {
        vga_print("[lua] Execution error: ");
        const char* err = lua_tostring(g_lua, -1);
        if (err) vga_print(err);
        vga_print("\n");
        lua_pop(g_lua, 1);
        return 0;
    }

    vga_print("[lua] executed OK\n");
    return 1;
}

int klua_get_global_table_string(const char* table_name, const char* key, char* out, unsigned int out_size) {
    int ok = 0;
    const char* value;

    if (!g_lua || !table_name || !key || !out || out_size == 0) {
        return 0;
    }

    lua_getglobal(g_lua, table_name);
    if (!lua_istable(g_lua, -1)) {
        lua_pop(g_lua, 1);
        return 0;
    }

    lua_getfield(g_lua, -1, key);
    value = lua_tostring(g_lua, -1);

    if (value) {
        klua_copy_string(out, out_size, value);
        ok = 1;
    }

    lua_pop(g_lua, 2);
    return ok;
}

unsigned int klua_get_global_table_count(const char* table_name) {
    unsigned int count = 0;

    if (!g_lua || !table_name) {
        return 0;
    }

    lua_getglobal(g_lua, table_name);
    if (!lua_istable(g_lua, -1)) {
        lua_pop(g_lua, 1);
        return 0;
    }

    lua_pushnil(g_lua);
    while (lua_next(g_lua, -2) != 0) {
        lua_pop(g_lua, 1);
        count++;
    }

    lua_pop(g_lua, 1);
    return count;
}

int klua_get_global_table_key_at(const char* table_name, unsigned int index, char* out, unsigned int out_size) {
    unsigned int i = 0;

    if (!g_lua || !table_name || !out || out_size == 0) {
        return 0;
    }

    lua_getglobal(g_lua, table_name);
    if (!lua_istable(g_lua, -1)) {
        lua_pop(g_lua, 1);
        return 0;
    }

    lua_pushnil(g_lua);
    while (lua_next(g_lua, -2) != 0) {
        if (i == index) {
            const char* key = lua_tostring(g_lua, -2);
            if (key) {
                klua_copy_string(out, out_size, key);
                lua_pop(g_lua, 2);
                return 1;
            }

            lua_pop(g_lua, 2);
            return 0;
        }

        lua_pop(g_lua, 1);
        i++;
    }

    lua_pop(g_lua, 1);
    return 0;
}
