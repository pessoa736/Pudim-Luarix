#include "klua_cmds.h"
#include "klua.h"
#include "kfs.h"

#define KLUA_CMD_FILE "commands.lua"
#define KLUA_CMD_TABLE "KCMDS"
#define KLUA_CMD_MAX_NAME 64
#define KLUA_CMD_MAX_SCRIPT 512
#define KLUA_CMD_MAX_TOTAL_SIZE 65536

static const char* g_klua_default_file =
#include "commands_lua.inc"
    ;

static int klua_cmd_is_valid_name(const char* name) {
    unsigned int i = 0;

    if (!name || !name[0]) {
        return 0;
    }

    while (name[i]) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_')) {
            return 0;
        }
        i++;

        if (i > KLUA_CMD_MAX_NAME) {
            return 0;
        }
    }

    return i > 0 && i < KLUA_CMD_MAX_NAME;
}

static int klua_cmd_ensure_file(void) {
    if (!kfs_exists(KLUA_CMD_FILE)) {
        return kfs_write(KLUA_CMD_FILE, g_klua_default_file);
    }

    return 1;
}

static int klua_cmd_refresh(void) {
    const char* file_code;

    if (!klua_cmd_ensure_file()) {
        return 0;
    }

    file_code = kfs_read(KLUA_CMD_FILE);
    if (!file_code) {
        return 0;
    }

    return klua_run(file_code);
}

unsigned int klua_cmd_count(void) {
    if (!klua_cmd_refresh()) {
        return 0;
    }

    return klua_get_global_table_count(KLUA_CMD_TABLE);
}

const char* klua_cmd_name_at(unsigned int index) {
    static char key_buf[KLUA_CMD_MAX_NAME];

    if (!klua_cmd_refresh()) {
        return 0;
    }

    if (!klua_get_global_table_key_at(KLUA_CMD_TABLE, index, key_buf, sizeof(key_buf))) {
        return 0;
    }

    return key_buf;
}

const char* klua_cmd_get_script(const char* name) {
    static char script_buf[KLUA_CMD_MAX_SCRIPT];

    if (!name) {
        return 0;
    }

    if (!klua_cmd_refresh()) {
        return 0;
    }

    if (!klua_get_global_table_string(KLUA_CMD_TABLE, name, script_buf, sizeof(script_buf))) {
        return 0;
    }

    return script_buf;
}

int klua_cmd_run(const char* name) {
    const char* script;
    unsigned int i = 0;

    if (!klua_cmd_is_valid_name(name)) {
        return 0;
    }

    script = klua_cmd_get_script(name);

    if (!script) {
        return 0;
    }

    while (script[i]) {
        i++;
    }

    if (i > KLUA_CMD_MAX_SCRIPT) {
        return 0;
    }

    return klua_run(script);
}
