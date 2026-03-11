#include "ksynclua.h"
#include "ksync.h"

static int ksynclua_spinlock_lock(lua_State* L);
static int ksynclua_spinlock_unlock(lua_State* L);
static int ksynclua_mutex_lock(lua_State* L);
static int ksynclua_mutex_unlock(lua_State* L);

static int ksynclua_spinlock_new(lua_State* L) {
    ksync_spinlock_t* lock = ksync_spinlock_create();
    ksync_spinlock_t** lock_p;

    if (!lock) {
        lua_pushstring(L, "spinlock creation failed");
        lua_error(L);
        return 0;
    }

    lock_p = (ksync_spinlock_t**)lua_newuserdata(L, sizeof(ksync_spinlock_t*));
    *lock_p = lock;

    lua_createtable(L, 0, 4);
    lua_pushcfunction(L, ksynclua_spinlock_lock);
    lua_setfield(L, -2, "lock");
    lua_pushcfunction(L, ksynclua_spinlock_unlock);
    lua_setfield(L, -2, "unlock");
    lua_pushcfunction(L, ksynclua_spinlock_lock);
    lua_setfield(L, -2, "acquire");
    lua_pushcfunction(L, ksynclua_spinlock_unlock);
    lua_setfield(L, -2, "release");
    lua_setmetatable(L, -2);

    return 1;
}

static int ksynclua_spinlock_lock(lua_State* L) {
    ksync_spinlock_t** lock_p = (ksync_spinlock_t**)lua_touserdata(L, 1);

    if (!lock_p || !*lock_p) {
        lua_pushstring(L, "spinlock:lock() called on invalid lock");
        lua_error(L);
        return 0;
    }

    ksync_spinlock_lock(*lock_p);
    return 0;
}

static int ksynclua_spinlock_unlock(lua_State* L) {
    ksync_spinlock_t** lock_p = (ksync_spinlock_t**)lua_touserdata(L, 1);

    if (!lock_p || !*lock_p) {
        lua_pushstring(L, "spinlock:unlock() called on invalid lock");
        lua_error(L);
        return 0;
    }

    ksync_spinlock_unlock(*lock_p);
    return 0;
}

static int ksynclua_mutex_new(lua_State* L) {
    ksync_mutex_t* lock = ksync_mutex_create();
    ksync_mutex_t** lock_p;

    if (!lock) {
        lua_pushstring(L, "mutex creation failed");
        lua_error(L);
        return 0;
    }

    lock_p = (ksync_mutex_t**)lua_newuserdata(L, sizeof(ksync_mutex_t*));
    *lock_p = lock;

    lua_createtable(L, 0, 4);
    lua_pushcfunction(L, ksynclua_mutex_lock);
    lua_setfield(L, -2, "lock");
    lua_pushcfunction(L, ksynclua_mutex_unlock);
    lua_setfield(L, -2, "unlock");
    lua_pushcfunction(L, ksynclua_mutex_lock);
    lua_setfield(L, -2, "acquire");
    lua_pushcfunction(L, ksynclua_mutex_unlock);
    lua_setfield(L, -2, "release");
    lua_setmetatable(L, -2);

    return 1;
}

static int ksynclua_mutex_lock(lua_State* L) {
    ksync_mutex_t** lock_p = (ksync_mutex_t**)lua_touserdata(L, 1);

    if (!lock_p || !*lock_p) {
        lua_pushstring(L, "mutex:lock() called on invalid lock");
        lua_error(L);
        return 0;
    }

    ksync_mutex_lock(*lock_p);
    return 0;
}

static int ksynclua_mutex_unlock(lua_State* L) {
    ksync_mutex_t** lock_p = (ksync_mutex_t**)lua_touserdata(L, 1);

    if (!lock_p || !*lock_p) {
        lua_pushstring(L, "mutex:unlock() called on invalid lock");
        lua_error(L);
        return 0;
    }

    ksync_mutex_unlock(*lock_p);
    return 0;
}

int ksynclua_register(lua_State* L) {
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, ksynclua_spinlock_new);
    lua_setfield(L, -2, "spinlock");
    lua_pushcfunction(L, ksynclua_mutex_new);
    lua_setfield(L, -2, "mutex");
    lua_setglobal(L, "sync");
    return 1;
}
