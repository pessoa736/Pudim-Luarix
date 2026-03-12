#include "klua.h"
#include "kheap.h"
#include "kio.h"
#include "kfslua.h"
#include "keventlua.h"
#include "kformatlua.h"
#include "kkeyboardlua.h"
#include "kmemorylua.h"
#include "kprocesslua.h"
#include "ksyslua.h"
#include "ksynclua.h"
#include "kdebuglua.h"
#include "serial.h"
#include "kdisplay.h"
#include "arch.h"

#if ARCH_HAS_VGA
#include "vga.h"
#endif

#include "../lua/src/lua.h"

static lua_State* g_lua = NULL;

lua_State* klua_state(void) {
    return g_lua;
}

#define KLUA_CALL_MAX_ARGS 8u
#define KLUA_CALL_MAX_ARG_LEN 96u

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

static void klua_integer_to_string(lua_Integer value, char* buf, unsigned int bufsize);

static int klua_panic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    }
#endif

    kdisplay_write("[lua panic] ");

    if (msg) {
        kdisplay_write(msg);
    } else {
        kdisplay_write("(no message)");
    }

    kdisplay_write("\n");

    /* Mirror to serial when VGA is primary */
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print("[lua panic] ");
        serial_print(msg ? msg : "(no message)");
        serial_print("\r\n");
    }

    /* Print Lua stack traceback */
    {
        lua_Debug ar;
        int level;

#if ARCH_HAS_VGA
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            vga_set_color(VGA_YELLOW, VGA_BLACK);
        }
#endif

        kdisplay_write("  Lua traceback:\n");

        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            serial_print("  Lua traceback:\r\n");
        }

#if ARCH_HAS_VGA
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        }
#endif

        for (level = 0; level < 16; level++) {
            if (!lua_getstack(L, level, &ar)) {
                break;
            }

            lua_getinfo(L, "Snl", &ar);

            kdisplay_write("    ");

            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print("    ");
            }

            if (ar.source && ar.source[0]) {
                kdisplay_write(ar.source);
                if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                    serial_print(ar.source);
                }
            }

            if (ar.currentline > 0) {
                char linebuf[16];
                klua_integer_to_string((lua_Integer)ar.currentline, linebuf, sizeof(linebuf));
                kdisplay_write(":");
                kdisplay_write(linebuf);
                if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                    serial_print(":");
                    serial_print(linebuf);
                }
            }

            kdisplay_write(": ");

            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print(": ");
            }

            if (ar.name && ar.name[0]) {
                kdisplay_write("in '");
                kdisplay_write(ar.name);
                kdisplay_write("'");
                if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                    serial_print("in '");
                    serial_print(ar.name);
                    serial_print("'");
                }
            } else if (ar.what && ar.what[0]) {
                kdisplay_write("in ");
                kdisplay_write(ar.what);
                if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                    serial_print("in ");
                    serial_print(ar.what);
                }
            }

            kdisplay_write("\n");

            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print("\r\n");
            }
        }
    }

    while (1) {
        arch_halt();
    }
}

static void klua_integer_to_string(lua_Integer value, char* buf, unsigned int bufsize) {
    lua_Unsigned magnitude;
    char tmp[32];
    unsigned int n = 0;
    unsigned int p = 0;

    if (!buf || bufsize < 2) {
        return;
    }

    if (value < 0) {
        magnitude = (lua_Unsigned)(-(value + 1)) + 1u;
    } else {
        magnitude = (lua_Unsigned)value;
    }

    if (value < 0 && p + 1 < bufsize) {
        buf[p++] = '-';
    }

    if (magnitude == 0) {
        buf[p++] = '0';
        buf[p] = 0;
        return;
    }

    while (magnitude > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    }

    while (n > 0 && p + 1 < bufsize) {
        buf[p++] = tmp[--n];
    }

    buf[p] = 0;
}

static void klua_write_value(lua_State* L, int idx) {
    int type = lua_type(L, idx);

    if (type == LUA_TSTRING) {
        const char* str = lua_tostring(L, idx);
        if (str) {
            kio_write(str);
        }
    } else if (type == LUA_TNUMBER) {
        char buf[64];
        if (lua_isinteger(L, idx)) {
            klua_integer_to_string(lua_tointeger(L, idx), buf, sizeof(buf));
        } else if (lua_numbertocstring(L, idx, buf) == 0) {
            buf[0] = '?';
            buf[1] = 0;
        }
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

/* --- Lua base library functions (no lbaselib.c) --- */

static int klua_tonumber(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        lua_settop(L, 1);
        return 1;
    }

    if (lua_type(L, 1) == LUA_TSTRING) {
        if (lua_stringtonumber(L, lua_tostring(L, 1))) {
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

static int klua_tostring(lua_State* L) {
    int type = lua_type(L, 1);

    if (type == LUA_TSTRING) {
        lua_settop(L, 1);
        return 1;
    }

    if (type == LUA_TNUMBER) {
        lua_pushstring(L, lua_tostring(L, 1));
        return 1;
    }

    if (type == LUA_TBOOLEAN) {
        lua_pushstring(L, lua_toboolean(L, 1) ? "true" : "false");
        return 1;
    }

    if (type == LUA_TNIL) {
        lua_pushstring(L, "nil");
        return 1;
    }

    lua_pushstring(L, lua_typename(L, type));
    return 1;
}

static int klua_type(lua_State* L) {
    lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
    return 1;
}

static int klua_next(lua_State* L) {
    lua_settop(L, 2);
    if (lua_next(L, 1)) {
        return 2;
    }

    lua_pushnil(L);
    return 1;
}

static int klua_pairs_iter(lua_State* L) {
    lua_settop(L, 2);
    if (lua_next(L, 1)) {
        return 2;
    }

    lua_pushnil(L);
    return 1;
}

static int klua_pairs(lua_State* L) {
    lua_pushcfunction(L, klua_pairs_iter);
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    return 3;
}

static int klua_ipairs_iter(lua_State* L) {
    lua_Integer i = lua_tointeger(L, 2) + 1;
    lua_pushinteger(L, i);
    lua_geti(L, 1, i);
    return lua_isnil(L, -1) ? 0 : 2;
}

static int klua_ipairs(lua_State* L) {
    lua_pushcfunction(L, klua_ipairs_iter);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, 0);
    return 3;
}

static int klua_select(lua_State* L) {
    int n = lua_gettop(L);

    if (lua_type(L, 1) == LUA_TSTRING) {
        const char* s = lua_tostring(L, 1);
        if (s && s[0] == '#') {
            lua_pushinteger(L, n - 1);
            return 1;
        }
    }

    if (lua_isinteger(L, 1)) {
        lua_Integer idx = lua_tointeger(L, 1);
        if (idx < 0) {
            idx = n + idx;
        }

        if (idx < 1 || idx >= n) {
            return 0;
        }

        return n - (int)idx;
    }

    return 0;
}

static int klua_error(lua_State* L) {
    return lua_error(L);
}

static int klua_pcall(lua_State* L) {
    int nargs = lua_gettop(L) - 1;
    int status;

    if (nargs < 0) {
        nargs = 0;
    }

    status = lua_pcall(L, nargs, LUA_MULTRET, 0);
    lua_pushboolean(L, status == LUA_OK);
    lua_insert(L, 1);
    return lua_gettop(L);
}

static int klua_rawget(lua_State* L) {
    lua_settop(L, 2);
    lua_rawget(L, 1);
    return 1;
}

static int klua_rawset(lua_State* L) {
    lua_settop(L, 3);
    lua_rawset(L, 1);
    return 0;
}

static int klua_rawlen(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)lua_rawlen(L, 1));
    return 1;
}

static int klua_setmetatable(lua_State* L) {
    lua_settop(L, 2);
    lua_setmetatable(L, 1);
    lua_settop(L, 1);
    return 1;
}

static int klua_getmetatable(lua_State* L) {
    if (!lua_getmetatable(L, 1)) {
        lua_pushnil(L);
    }

    return 1;
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

    /* Register base library functions */
    lua_pushcfunction(g_lua, klua_tonumber);
    lua_setglobal(g_lua, "tonumber");
    lua_pushcfunction(g_lua, klua_tostring);
    lua_setglobal(g_lua, "tostring");
    lua_pushcfunction(g_lua, klua_type);
    lua_setglobal(g_lua, "type");
    lua_pushcfunction(g_lua, klua_next);
    lua_setglobal(g_lua, "next");
    lua_pushcfunction(g_lua, klua_pairs);
    lua_setglobal(g_lua, "pairs");
    lua_pushcfunction(g_lua, klua_ipairs);
    lua_setglobal(g_lua, "ipairs");
    lua_pushcfunction(g_lua, klua_select);
    lua_setglobal(g_lua, "select");
    lua_pushcfunction(g_lua, klua_error);
    lua_setglobal(g_lua, "error");
    lua_pushcfunction(g_lua, klua_pcall);
    lua_setglobal(g_lua, "pcall");
    lua_pushcfunction(g_lua, klua_rawget);
    lua_setglobal(g_lua, "rawget");
    lua_pushcfunction(g_lua, klua_rawset);
    lua_setglobal(g_lua, "rawset");
    lua_pushcfunction(g_lua, klua_rawlen);
    lua_setglobal(g_lua, "rawlen");
    lua_pushcfunction(g_lua, klua_setmetatable);
    lua_setglobal(g_lua, "setmetatable");
    lua_pushcfunction(g_lua, klua_getmetatable);
    lua_setglobal(g_lua, "getmetatable");

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

    if (!kmemorylua_register(g_lua)) {
        return 0;
    }

    if (!kprocesslua_register(g_lua)) {
        return 0;
    }

    if (!kkeyboardlua_register(g_lua)) {
        return 0;
    }

    if (!kformatlua_register(g_lua)) {
        return 0;
    }

    if (!keventlua_register(g_lua)) {
        return 0;
    }

    if (!kdebuglua_register(g_lua)) {
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

/* Traceback message handler for lua_pcall */
static int klua_msgh(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    lua_Debug ar;
    int level;
    int first = 1;

    if (msg) {
        lua_pushstring(L, msg);
    } else {
        lua_pushstring(L, "(error object is not a string)");
    }

    lua_pushstring(L, "\n  traceback:");

    for (level = 1; level < 16; level++) {
        char linebuf[16];

        if (!lua_getstack(L, level, &ar)) {
            break;
        }

        lua_getinfo(L, "Snl", &ar);

        if (first) {
            first = 0;
        }

        lua_pushstring(L, "\n    ");

        if (ar.source && ar.source[0]) {
            lua_pushstring(L, ar.source);
        } else {
            lua_pushstring(L, "?");
        }

        if (ar.currentline > 0) {
            klua_integer_to_string((lua_Integer)ar.currentline, linebuf, sizeof(linebuf));
            lua_pushstring(L, ":");
            lua_pushstring(L, linebuf);
        }

        lua_pushstring(L, ": ");

        if (ar.name && ar.name[0]) {
            lua_pushstring(L, "in '");
            lua_pushstring(L, ar.name);
            lua_pushstring(L, "'");
        } else if (ar.what && ar.what[0]) {
            lua_pushstring(L, "in ");
            lua_pushstring(L, ar.what);
        }

        lua_concat(L, lua_gettop(L));
    }

    lua_concat(L, lua_gettop(L));
    return 1;
}

static int klua_run_internal(const char* code, int announce_success) {
    int base;
    int msgh_index;

    if (!g_lua) {
        kdisplay_write("lua: VM not initialized\n");
        return 0;
    }

    if (!code) {
        kdisplay_write("lua: no code to execute\n");
        return 0;
    }

    base = lua_gettop(g_lua);

    /* Push message handler for traceback on error */
    lua_pushcfunction(g_lua, klua_msgh);
    msgh_index = lua_gettop(g_lua);

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
        kdisplay_write("lua load error: ");
        const char* err = lua_tostring(g_lua, -1);
        if (err) kdisplay_write(err);
        kdisplay_write("\n");
        lua_settop(g_lua, base);
        return 0;
    }

    /* Execute the loaded function with message handler */
    int call_result = lua_pcall(g_lua, 0, 0, msgh_index);
    if (call_result != LUA_OK) {
        kdisplay_write("lua error: ");
        const char* err = lua_tostring(g_lua, -1);
        if (err) kdisplay_write(err);
        kdisplay_write("\n");
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            serial_print("[lua error] ");
            err = lua_tostring(g_lua, -1);
            if (err) serial_print(err);
            serial_print("\r\n");
        }
        lua_settop(g_lua, base);
        return 0;
    }

    lua_settop(g_lua, base);

    if (announce_success) {
        kdisplay_write("lua executed OK\n");
    }

    return 1;
}

int klua_run(const char* code) {
    return klua_run_internal(code, 1);
}

int klua_run_quiet(const char* code) {
    return klua_run_internal(code, 0);
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

int klua_call_global_table_function(const char* table_name, const char* key) {
    return klua_call_global_table_function_with_args(table_name, key, 0);
}

int klua_call_global_table_function_with_args(const char* table_name, const char* key, const char* args_line) {
    int call_result;
    unsigned int argc = 0;

    if (!g_lua || !table_name || !key) {
        return 0;
    }

    lua_getglobal(g_lua, table_name);
    if (!lua_istable(g_lua, -1)) {
        lua_pop(g_lua, 1);
        return 0;
    }

    lua_getfield(g_lua, -1, key);
    if (!lua_isfunction(g_lua, -1)) {
        lua_pop(g_lua, 2);
        return 0;
    }

    if (args_line && args_line[0]) {
        unsigned int i = 0;

        while (args_line[i] && argc < KLUA_CALL_MAX_ARGS) {
            char token[KLUA_CALL_MAX_ARG_LEN];
            unsigned int tlen = 0;
            char quote = 0;

            while (args_line[i] == ' ') {
                i++;
            }

            if (!args_line[i]) {
                break;
            }

            if (args_line[i] == '"' || args_line[i] == '\'') {
                quote = args_line[i++];
            }

            while (args_line[i] && tlen + 1 < KLUA_CALL_MAX_ARG_LEN) {
                if (quote) {
                    if (args_line[i] == quote) {
                        i++;
                        break;
                    }
                } else if (args_line[i] == ' ') {
                    break;
                }

                token[tlen++] = args_line[i++];
            }

            token[tlen] = 0;

            if (quote) {
                while (args_line[i] && args_line[i] != ' ') {
                    i++;
                }
            }

            if (tlen > 0) {
                lua_pushstring(g_lua, token);
                argc++;
            }
        }
    }

    call_result = lua_pcall(g_lua, (int)argc, 0, 0);
    if (call_result != LUA_OK) {
        kdisplay_write("lua command error: ");
        {
            const char* err = lua_tostring(g_lua, -1);
            if (err) {
                kdisplay_write(err);
            }
        }
        kdisplay_write("\n");
        lua_pop(g_lua, 2);
        return 0;
    }

    lua_pop(g_lua, 1);
    return 1;
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
