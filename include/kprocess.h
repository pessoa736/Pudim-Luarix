#ifndef KPROCESS_H
#define KPROCESS_H

#include "utypes.h"
#include "../lua/src/lua.h"

#define KPROCESS_MAX_PROCESSES 16
#define KPROCESS_MAX_SCRIPT_SIZE 1024

typedef enum {
    KPROCESS_STATE_DEAD = 0,
    KPROCESS_STATE_READY = 1,
    KPROCESS_STATE_RUNNING = 2,
    KPROCESS_STATE_ERROR = 3
} kprocess_state_t;

int kprocess_init(lua_State* main_lua);
int kprocess_create(const char* script);
int kprocess_create_function(lua_State* owner, int func_index);
int kprocess_kill(unsigned int pid);
unsigned int kprocess_current_pid(lua_State* L);
unsigned int kprocess_count(void);
unsigned int kprocess_max(void);
unsigned int kprocess_pid_at(unsigned int index);
kprocess_state_t kprocess_state_of(unsigned int pid);
int kprocess_tick(void);
void kprocess_request_tick(void);
int kprocess_poll(void);

unsigned int kprocess_get_uid(unsigned int pid);
unsigned int kprocess_get_gid(unsigned int pid);
int kprocess_set_uid(unsigned int pid, unsigned int uid);
int kprocess_set_gid(unsigned int pid, unsigned int gid);

#endif
