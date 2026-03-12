#include "kkeyboardlua.h"
#include "keyboard.h"

static int kkeyboardlua_getkey(lua_State* L) {
    unsigned char key;

    if (keyboard_getkey_nonblock(&key)) {
        if (key >= 0x80) {
            /* Special key: return name string */
            const char* name = NULL;
            switch (key) {
                case KBD_KEY_UP:     name = "up"; break;
                case KBD_KEY_DOWN:   name = "down"; break;
                case KBD_KEY_LEFT:   name = "left"; break;
                case KBD_KEY_RIGHT:  name = "right"; break;
                case KBD_KEY_HOME:   name = "home"; break;
                case KBD_KEY_END:    name = "end"; break;
                case KBD_KEY_DELETE: name = "delete"; break;
                default: break;
            }
            if (name) {
                lua_pushstring(L, name);
            } else {
                lua_pushnil(L);
            }
        } else {
            /* Regular ASCII: return single-char string */
            char buf[2];
            buf[0] = (char)key;
            buf[1] = 0;
            lua_pushstring(L, buf);
        }
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

static int kkeyboardlua_getchar(lua_State* L) {
    char c;

    if (keyboard_getchar_nonblock(&c)) {
        char buf[2];
        buf[0] = c;
        buf[1] = 0;
        lua_pushstring(L, buf);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int kkeyboardlua_available(lua_State* L) {
    lua_pushboolean(L, keyboard_available());
    return 1;
}

int kkeyboardlua_register(lua_State* L) {
    if (!L) {
        return 0;
    }

    lua_createtable(L, 0, 3);

    lua_pushcfunction(L, kkeyboardlua_getkey);
    lua_setfield(L, -2, "getkey");

    lua_pushcfunction(L, kkeyboardlua_getchar);
    lua_setfield(L, -2, "getchar");

    lua_pushcfunction(L, kkeyboardlua_available);
    lua_setfield(L, -2, "available");

    lua_setglobal(L, "keyboard");
    return 1;
}
