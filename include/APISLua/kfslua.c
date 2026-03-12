#include "kfslua.h"
#include "kfs.h"
#include "kprocess.h"
#include "ksecurity.h"

static int g_kfs_ready = 0;

static void kfslua_ensure_init(void) {
    if (!g_kfs_ready) {
        kfs_init();
        g_kfs_ready = 1;
    }
}

static const char* kfslua_check_string_arg(lua_State* L, int idx) {
    if (!lua_isstring(L, idx)) {
        return NULL;
    }

    return lua_tostring(L, idx);
}

static int kfslua_write(lua_State* L) {
    const char* name;
    const char* content;
    int ok;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    content = kfslua_check_string_arg(L, 2);

    if (!name || !content) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ok = kfs_write(name, content);
    lua_pushboolean(L, ok);
    return 1;
}

static int kfslua_append(lua_State* L) {
    const char* name;
    const char* content;
    int ok;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    content = kfslua_check_string_arg(L, 2);

    if (!name || !content) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ok = kfs_append(name, content);
    lua_pushboolean(L, ok);
    return 1;
}

static int kfslua_read(lua_State* L) {
    const char* name;
    const char* content;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name) {
        lua_pushnil(L);
        return 1;
    }

    content = kfs_read(name);

    if (!content) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, content);
    return 1;
}

static int kfslua_delete(lua_State* L) {
    const char* name;
    int ok;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ok = kfs_delete(name);
    lua_pushboolean(L, ok);
    return 1;
}

static int kfslua_exists(lua_State* L) {
    const char* name;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kfs_exists(name));
    return 1;
}

static int kfslua_size(lua_State* L) {
    const char* name;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)kfs_size(name));
    return 1;
}

static int kfslua_count(lua_State* L) {
    (void)L;

    kfslua_ensure_init();

    lua_pushinteger(L, (lua_Integer)kfs_count());
    return 1;
}

static int kfslua_list(lua_State* L) {
    size_t count;
    size_t i;

    kfslua_ensure_init();

    count = kfs_count();
    lua_createtable(L, (int)count, 0);

    for (i = 0; i < count; i++) {
        const char* name = kfs_name_at(i);
        if (!name) {
            continue;
        }

        lua_pushstring(L, name);
        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }

    return 1;
}

static int kfslua_clear(lua_State* L) {
    (void)L;

    kfslua_ensure_init();
    kfs_clear();

    lua_pushboolean(L, 1);
    return 1;
}

static int kfslua_save(lua_State* L) {
    (void)L;

    kfslua_ensure_init();
    lua_pushboolean(L, kfs_persist_save());
    return 1;
}

static int kfslua_load(lua_State* L) {
    (void)L;

    kfslua_ensure_init();
    kfs_clear();
    lua_pushboolean(L, kfs_persist_load());
    return 1;
}

static int kfslua_persistent(lua_State* L) {
    (void)L;

    lua_pushboolean(L, kfs_persist_available());
    return 1;
}

static int kfslua_chmod(lua_State* L) {
    const char* name;
    lua_Integer mode;
    unsigned int pid;
    unsigned int caller_uid;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name || lua_gettop(L) < 2 || !lua_isinteger(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    mode = lua_tointeger(L, 2);
    if (mode < 0 || mode > 0777) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!kfs_exists(name)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    pid = kprocess_current_pid(L);
    caller_uid = (pid == 0) ? 0 : kprocess_get_uid(pid);

    /* Only root, file owner, or CAP_CHMOD can chmod */
    if (caller_uid != KSECURITY_ROOT_UID &&
        caller_uid != kfs_get_owner(name) &&
        !ksecurity_check_capability(caller_uid, KSECURITY_CAP_CHMOD)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kfs_chmod(name, (unsigned int)mode));
    return 1;
}

static int kfslua_chown(lua_State* L) {
    const char* name;
    lua_Integer uid;
    lua_Integer gid;
    unsigned int pid;
    unsigned int caller_uid;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name || lua_gettop(L) < 3 || !lua_isinteger(L, 2) || !lua_isinteger(L, 3)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    uid = lua_tointeger(L, 2);
    gid = lua_tointeger(L, 3);
    if (uid < 0 || gid < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!kfs_exists(name)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    pid = kprocess_current_pid(L);
    caller_uid = (pid == 0) ? 0 : kprocess_get_uid(pid);

    /* Only root or CAP_CHOWN can chown */
    if (caller_uid != KSECURITY_ROOT_UID &&
        !ksecurity_check_capability(caller_uid, KSECURITY_CAP_CHOWN)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, kfs_chown(name, (unsigned int)uid, (unsigned int)gid));
    return 1;
}

static int kfslua_getperm(lua_State* L) {
    const char* name;

    kfslua_ensure_init();

    name = kfslua_check_string_arg(L, 1);
    if (!name || !kfs_exists(name)) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 3);

    lua_pushinteger(L, (lua_Integer)kfs_get_mode(name));
    lua_setfield(L, -2, "mode");

    lua_pushinteger(L, (lua_Integer)kfs_get_owner(name));
    lua_setfield(L, -2, "uid");

    lua_pushinteger(L, (lua_Integer)kfs_get_group(name));
    lua_setfield(L, -2, "gid");

    return 1;
}

int kfslua_register(lua_State* L) {
    if (!L) {
        return 0;
    }

    kfslua_ensure_init();

    lua_createtable(L, 0, 15);

    lua_pushcfunction(L, kfslua_write);
    lua_setfield(L, -2, "write");

    lua_pushcfunction(L, kfslua_append);
    lua_setfield(L, -2, "append");

    lua_pushcfunction(L, kfslua_read);
    lua_setfield(L, -2, "read");

    lua_pushcfunction(L, kfslua_delete);
    lua_setfield(L, -2, "delete");

    lua_pushcfunction(L, kfslua_exists);
    lua_setfield(L, -2, "exists");

    lua_pushcfunction(L, kfslua_size);
    lua_setfield(L, -2, "size");

    lua_pushcfunction(L, kfslua_count);
    lua_setfield(L, -2, "count");

    lua_pushcfunction(L, kfslua_list);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, kfslua_clear);
    lua_setfield(L, -2, "clear");

    lua_pushcfunction(L, kfslua_save);
    lua_setfield(L, -2, "save");

    lua_pushcfunction(L, kfslua_load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, kfslua_persistent);
    lua_setfield(L, -2, "persistent");

    lua_pushcfunction(L, kfslua_chmod);
    lua_setfield(L, -2, "chmod");

    lua_pushcfunction(L, kfslua_chown);
    lua_setfield(L, -2, "chown");

    lua_pushcfunction(L, kfslua_getperm);
    lua_setfield(L, -2, "getperm");

    lua_pushvalue(L, -1);
    lua_setglobal(L, "fs");

    lua_setglobal(L, "kfs");

    return 1;
}
