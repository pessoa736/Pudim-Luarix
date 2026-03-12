#include "klua_cmds.h"
#include "klua.h"
#include "kfs.h"

#define KLUA_CMD_FILE "commands.lua"
#define KLUA_CMD_TABLE "KCMDS"
#define KLUA_CMD_MAX_NAME 64
#define KLUA_CMD_MAX_SCRIPT 512
#define KLUA_CMD_MAX_TOTAL_SIZE 65536
#define KLUA_CMD_MAX_LINE 250
#define KLUA_CMD_MAX_INVOKE 512
#define KLUA_CMD_MAX_ARGS 8
#define KLUA_CMD_MAX_ARG_LEN 96

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

    return klua_run_quiet(file_code);
}

static int klua_cmd_append_text(char* out, unsigned int out_cap, unsigned int* pos, const char* text) {
    unsigned int i = 0;

    if (!out || !pos || !text || out_cap == 0) {
        return 0;
    }

    while (text[i]) {
        if (*pos + 1 >= out_cap) {
            return 0;
        }
        out[*pos] = text[i];
        (*pos)++;
        i++;
    }

    out[*pos] = 0;
    return 1;
}

static int klua_cmd_append_quoted(char* out, unsigned int out_cap, unsigned int* pos, const char* text) {
    unsigned int i = 0;

    if (!klua_cmd_append_text(out, out_cap, pos, "\"")) {
        return 0;
    }

    while (text && text[i]) {
        char c = text[i++];
        if (c == '\\' || c == '"') {
            if (*pos + 2 >= out_cap) {
                return 0;
            }
            out[(*pos)++] = '\\';
            out[(*pos)++] = c;
        } else {
            if (*pos + 1 >= out_cap) {
                return 0;
            }
            out[(*pos)++] = c;
        }
    }

    out[*pos] = 0;
    return klua_cmd_append_text(out, out_cap, pos, "\"");
}

static int klua_cmd_build_invocation(const char* line, const char* name, const char* args, char* out, unsigned int out_cap) {
    unsigned int pos = 0;
    unsigned int argc = 0;
    unsigned int i = 0;

    (void)line;

    if (!name || !out || out_cap < 32) {
        return 0;
    }

    out[0] = 0;

    if (!klua_cmd_append_text(out, out_cap, &pos, "local __f=KCMDS[")) {
        return 0;
    }
    if (!klua_cmd_append_quoted(out, out_cap, &pos, name)) {
        return 0;
    }
    if (!klua_cmd_append_text(out, out_cap, &pos, "]; if __f then __f(")) {
        return 0;
    }

    while (args && args[i] && argc < KLUA_CMD_MAX_ARGS) {
        char token[KLUA_CMD_MAX_ARG_LEN];
        unsigned int tlen = 0;
        char quote = 0;

        while (args[i] == ' ') {
            i++;
        }

        if (!args[i]) {
            break;
        }

        if (args[i] == '"' || args[i] == '\'') {
            quote = args[i++];
        }

        while (args[i] && tlen + 1 < sizeof(token)) {
            if (quote) {
                if (args[i] == quote) {
                    i++;
                    break;
                }
            } else if (args[i] == ' ') {
                break;
            }

            token[tlen++] = args[i++];
        }
        token[tlen] = 0;

        if (quote) {
            while (args[i] && args[i] != ' ') {
                i++;
            }
        }

        if (tlen == 0) {
            continue;
        }

        if (argc > 0) {
            if (!klua_cmd_append_text(out, out_cap, &pos, ",")) {
                return 0;
            }
        }

        if (!klua_cmd_append_quoted(out, out_cap, &pos, token)) {
            return 0;
        }

        argc++;
    }

    return klua_cmd_append_text(out, out_cap, &pos, ") end");
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
        return klua_call_global_table_function(KLUA_CMD_TABLE, name);
    }

    while (script[i]) {
        i++;
    }

    if (i > KLUA_CMD_MAX_SCRIPT) {
        return 0;
    }

    return klua_run_quiet(script);
}

int klua_cmd_run_line(const char* line) {
    char name_buf[KLUA_CMD_MAX_NAME];
    char invoke_buf[KLUA_CMD_MAX_INVOKE];
    const char* args = 0;
    unsigned int i = 0;
    unsigned int p = 0;
    const char* script;

    if (!line || !line[0]) {
        return 0;
    }

    while (line[i] == ' ') {
        i++;
    }

    while (line[i] && line[i] != ' ' && p + 1 < sizeof(name_buf)) {
        name_buf[p++] = line[i++];
    }
    name_buf[p] = 0;

    while (line[i] == ' ') {
        i++;
    }

    if (line[i]) {
        args = &line[i];
    }

    if (!klua_cmd_is_valid_name(name_buf)) {
        return 0;
    }

    script = klua_cmd_get_script(name_buf);
    if (!script) {
        if (!klua_cmd_build_invocation(line, name_buf, args, invoke_buf, sizeof(invoke_buf))) {
            return 0;
        }

        return klua_run_quiet(invoke_buf);
    }

    i = 0;
    while (script[i]) {
        i++;
    }

    if (i > KLUA_CMD_MAX_SCRIPT) {
        return 0;
    }

    return klua_run_quiet(script);
}
