#ifndef KEVENTLUA_H
#define KEVENTLUA_H

#include "../lua/src/lua.h"

int keventlua_register(lua_State* L);

/* Dispatch pending events to Lua subscribers. Call from main loop. */
void keventlua_dispatch(lua_State* L);

#endif
