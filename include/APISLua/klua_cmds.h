#ifndef KLUA_CMDS_H
#define KLUA_CMDS_H

const char* klua_cmd_get_script(const char* name);
const char* klua_cmd_name_at(unsigned int index);
unsigned int klua_cmd_count(void);
int klua_cmd_run(const char* name);

#endif
