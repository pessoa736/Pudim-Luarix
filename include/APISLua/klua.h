#ifndef KLUA_H
#define KLUA_H

#include "../lua/src/lua.h"

lua_State* klua_state(void);
int klua_init(void);
int klua_run(const char* code);
int klua_run_quiet(const char* code);
int klua_get_global_table_string(const char* table_name, const char* key, char* out, unsigned int out_size);
int klua_call_global_table_function(const char* table_name, const char* key);
int klua_call_global_table_function_with_args(const char* table_name, const char* key, const char* args_line);
unsigned int klua_get_global_table_count(const char* table_name);
int klua_get_global_table_key_at(const char* table_name, unsigned int index, char* out, unsigned int out_size);

#endif
