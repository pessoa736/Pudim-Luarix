#include "kprocesslua.h"
#include "kprocess.h"
#include "ksecurity.h"

static int kprocesslua_create(lua_State* L) {
    const char* script;
    int pid;

    if (lua_gettop(L) != 1) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (lua_isfunction(L, 1)) {
        pid = kprocess_create_function(L, 1);
        lua_pushinteger(L, (lua_Integer)pid);
        return 1;
    }

    if (!lua_isstring(L, 1)) {
        lua_pushinteger(L, 0);
        return 1;
    }

    script = lua_tostring(L, 1);
    if (!script) {
        lua_pushinteger(L, 0);
        return 1;
    }

    pid = kprocess_create(script);
    lua_pushinteger(L, (lua_Integer)pid);
    return 1;
}

static int kprocesslua_list(lua_State* L) {
    unsigned int total = kprocess_count();
    unsigned int i;
    unsigned int out_index = 1;

    lua_createtable(L, (int)total, 0);

    for (i = 0; i < total; i++) {
        unsigned int pid = kprocess_pid_at(i);
        kprocess_state_t state;
        const char* state_name = "dead";

        if (pid == 0) {
            continue;
        }

        state = kprocess_state_of(pid);
        if (state == KPROCESS_STATE_READY) {
            state_name = "ready";
        } else if (state == KPROCESS_STATE_RUNNING) {
            state_name = "running";
        } else if (state == KPROCESS_STATE_ERROR) {
            state_name = "error";
        }

        lua_createtable(L, 0, 2);
        lua_pushinteger(L, (lua_Integer)pid);
        lua_setfield(L, -2, "pid");
        lua_pushstring(L, state_name);
        lua_setfield(L, -2, "state");

        lua_rawseti(L, -2, (lua_Integer)out_index);
        out_index++;
    }

    return 1;
}

static int kprocesslua_kill(lua_State* L) {
    lua_Integer pid;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    pid = lua_tointeger(L, 1);
    if (pid <= 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kprocess_kill((unsigned int)pid));
    return 1;
}

static int kprocesslua_yield(lua_State* L) {
    if (lua_gettop(L) != 0) {
        lua_settop(L, 0);
    }

    if (kprocess_current_pid(L) == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    return lua_yield(L, 0);
}

static int kprocesslua_get_id(lua_State* L) {
    unsigned int pid = kprocess_current_pid(L);
    lua_pushinteger(L, (lua_Integer)pid);
    return 1;
}

static int kprocesslua_tick(lua_State* L) {
    (void)L;
    lua_pushboolean(L, kprocess_tick());
    return 1;
}

static int kprocesslua_count(lua_State* L) {
    (void)L;
    lua_pushinteger(L, (lua_Integer)kprocess_count());
    return 1;
}

static int kprocesslua_max(lua_State* L) {
    (void)L;
    lua_pushinteger(L, (lua_Integer)kprocess_max());
    return 1;
}

static int kprocesslua_getuid(lua_State* L) {
    unsigned int pid = kprocess_current_pid(L);

    /* Terminal (pid=0) runs as root */
    if (pid == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)kprocess_get_uid(pid));
    return 1;
}

static int kprocesslua_setuid(lua_State* L) {
    unsigned int pid;
    unsigned int caller_uid;
    lua_Integer new_uid;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    new_uid = lua_tointeger(L, 1);
    if (new_uid < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!ksecurity_user_exists((unsigned int)new_uid)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    pid = kprocess_current_pid(L);
    caller_uid = (pid == 0) ? 0 : kprocess_get_uid(pid);

    /* Only root or users with CAP_SETUID can change uid */
    if (caller_uid != KSECURITY_ROOT_UID &&
        !ksecurity_check_capability(caller_uid, KSECURITY_CAP_SETUID)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (pid == 0) {
        /* Terminal: no process slot to set */
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kprocess_set_uid(pid, (unsigned int)new_uid));
    return 1;
}

static int kprocesslua_getgid(lua_State* L) {
    unsigned int pid = kprocess_current_pid(L);

    if (pid == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)kprocess_get_gid(pid));
    return 1;
}

static int kprocesslua_setgid(lua_State* L) {
    unsigned int pid;
    unsigned int caller_uid;
    lua_Integer new_gid;

    if (lua_gettop(L) != 1 || !lua_isinteger(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    new_gid = lua_tointeger(L, 1);
    if (new_gid < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    pid = kprocess_current_pid(L);
    caller_uid = (pid == 0) ? 0 : kprocess_get_uid(pid);

    if (caller_uid != KSECURITY_ROOT_UID &&
        !ksecurity_check_capability(caller_uid, KSECURITY_CAP_SETUID)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (pid == 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kprocess_set_gid(pid, (unsigned int)new_gid));
    return 1;
}

static int kprocesslua_get_capabilities(lua_State* L) {
    unsigned int pid = kprocess_current_pid(L);
    unsigned int uid = (pid == 0) ? 0 : kprocess_get_uid(pid);
    uint32_t caps = ksecurity_get_capabilities(uid);

    lua_createtable(L, 0, 6);

    lua_pushboolean(L, (caps & KSECURITY_CAP_SETUID) != 0);
    lua_setfield(L, -2, "setuid");

    lua_pushboolean(L, (caps & KSECURITY_CAP_CHMOD) != 0);
    lua_setfield(L, -2, "chmod");

    lua_pushboolean(L, (caps & KSECURITY_CAP_CHOWN) != 0);
    lua_setfield(L, -2, "chown");

    lua_pushboolean(L, (caps & KSECURITY_CAP_KILL) != 0);
    lua_setfield(L, -2, "kill");

    lua_pushboolean(L, (caps & KSECURITY_CAP_FS_ADMIN) != 0);
    lua_setfield(L, -2, "fs_admin");

    lua_pushboolean(L, (caps & KSECURITY_CAP_PROCESS_ADMIN) != 0);
    lua_setfield(L, -2, "process_admin");

    return 1;
}

int kprocesslua_register(lua_State* L) {
    if (!kprocess_init(L)) {
        return 0;
    }

    lua_createtable(L, 0, 13);

    lua_pushcfunction(L, kprocesslua_create);
    lua_setfield(L, -2, "create");

    lua_pushcfunction(L, kprocesslua_list);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, kprocesslua_kill);
    lua_setfield(L, -2, "kill");

    lua_pushcfunction(L, kprocesslua_yield);
    lua_setfield(L, -2, "yield");

    lua_pushcfunction(L, kprocesslua_get_id);
    lua_setfield(L, -2, "get_id");

    lua_pushcfunction(L, kprocesslua_tick);
    lua_setfield(L, -2, "tick");

    lua_pushcfunction(L, kprocesslua_count);
    lua_setfield(L, -2, "count");

    lua_pushcfunction(L, kprocesslua_max);
    lua_setfield(L, -2, "max");

    lua_pushcfunction(L, kprocesslua_getuid);
    lua_setfield(L, -2, "getuid");

    lua_pushcfunction(L, kprocesslua_setuid);
    lua_setfield(L, -2, "setuid");

    lua_pushcfunction(L, kprocesslua_getgid);
    lua_setfield(L, -2, "getgid");

    lua_pushcfunction(L, kprocesslua_setgid);
    lua_setfield(L, -2, "setgid");

    lua_pushcfunction(L, kprocesslua_get_capabilities);
    lua_setfield(L, -2, "get_capabilities");

    lua_setglobal(L, "process");
    return 1;
}
