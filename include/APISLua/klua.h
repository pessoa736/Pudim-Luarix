#ifndef KLUA_H
#define KLUA_H

int klua_init(void);
int klua_run(const char* code);
int klua_get_global_table_string(const char* table_name, const char* key, char* out, unsigned int out_size);
unsigned int klua_get_global_table_count(const char* table_name);
int klua_get_global_table_key_at(const char* table_name, unsigned int index, char* out, unsigned int out_size);

#endif
