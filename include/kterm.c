#include "kterm.h"
#include "kio.h"
#include "kfs.h"
#include "klua_cmds.h"
#include "serial.h"

#define KTERM_BUF_SIZE 256
#define KTERM_MAX_CMD_LEN 250
#define KTERM_TIMEOUT_MS 30000

static int kterm_streq(const char* a, const char* b) {
    int i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == b[i];
}

static int kterm_starts_with(const char* s, const char* prefix) {
    int i = 0;

    if (!s || !prefix) {
        return 0;
    }

    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

static size_t kterm_strlen(const char* s) {
    size_t len = 0;

    if (!s) {
        return 0;
    }

    while (s[len]) {
        len++;
    }

    return len;
}

static void kterm_u64_to_str(size_t value, char* out, size_t out_cap) {
    char tmp[32];
    size_t n = 0;
    size_t p = 0;

    if (!out || out_cap < 2) {
        return;
    }

    if (value == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    while (value > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (n > 0 && p + 1 < out_cap) {
        out[p++] = tmp[--n];
    }

    out[p] = 0;
}

static void kterm_print_prompt(void) {
    serial_print("kterm> ");
}

static void kterm_print_help(void) {
    serial_print("commands: help clear ls write read append rm count size lcmds lrun exit\r\n");
}

static void kterm_cmd_lcmds(void) {
    unsigned int total = klua_cmd_count();

    if (total == 0) {
        serial_print("(no lua commands)\r\n");
        return;
    }

    for (unsigned int i = 0; i < total; i++) {
        const char* name = klua_cmd_name_at(i);
        if (name) {
            serial_print(name);
            serial_print("\r\n");
        }
    }
}

static void kterm_cmd_lrun(const char* name) {
    if (!name || !name[0]) {
        serial_print("usage: lrun <lua_command_name>\r\n");
        return;
    }

    if (klua_cmd_run(name)) {
        serial_print("lua command executed\r\n");
    } else {
        serial_print("lua command not found or failed\r\n");
    }
}

static void kterm_cmd_ls(void) {
    size_t count = kfs_count();

    if (count == 0) {
        serial_print("(empty)\r\n");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const char* name = kfs_name_at(i);
        if (name) {
            serial_print(name);
            serial_print("\r\n");
        }
    }
}

static void kterm_cmd_count(void) {
    char buf[32];

    kterm_u64_to_str(kfs_count(), buf, sizeof(buf));
    serial_print(buf);
    serial_print("\r\n");
}

static void kterm_cmd_size(const char* name) {
    char buf[32];

    if (!name || !name[0]) {
        serial_print("usage: size <name>\r\n");
        return;
    }

    kterm_u64_to_str(kfs_size(name), buf, sizeof(buf));
    serial_print(buf);
    serial_print("\r\n");
}

static char* kterm_skip_cmd(char* line) {
    int i = 0;

    while (line[i] && line[i] != ' ') {
        i++;
    }

    while (line[i] == ' ') {
        i++;
    }

    return &line[i];
}

static void kterm_cmd_write(char* args) {
    char* name = args;
    char* content;
    int i = 0;

    while (args[i] && args[i] != ' ') {
        i++;
    }

    if (!args[0] || !args[i]) {
        serial_print("usage: write <name> <content>\r\n");
        return;
    }

    args[i] = 0;
    content = &args[i + 1];

    if (kfs_write(name, content)) {
        serial_print("ok\r\n");
    } else {
        serial_print("fail\r\n");
    }
}

static void kterm_cmd_append(char* args) {
    char* name = args;
    char* content;
    int i = 0;

    while (args[i] && args[i] != ' ') {
        i++;
    }

    if (!args[0] || !args[i]) {
        serial_print("usage: append <name> <content>\r\n");
        return;
    }

    args[i] = 0;
    content = &args[i + 1];

    if (kfs_append(name, content)) {
        serial_print("ok\r\n");
    } else {
        serial_print("fail\r\n");
    }
}

static void kterm_exec(char* line, int* keep_running) {
    char* args;

    if (!line[0]) {
        return;
    }

    if (kterm_streq(line, "help")) {
        kterm_print_help();
        return;
    }

    if (kterm_streq(line, "clear")) {
        kio_clear();
        serial_print("screen cleared\r\n");
        return;
    }

    if (kterm_streq(line, "ls")) {
        kterm_cmd_ls();
        return;
    }

    if (kterm_streq(line, "count")) {
        kterm_cmd_count();
        return;
    }

    if (kterm_streq(line, "lcmds")) {
        kterm_cmd_lcmds();
        return;
    }

    if (kterm_starts_with(line, "lrun ")) {
        args = kterm_skip_cmd(line);
        kterm_cmd_lrun(args);
        return;
    }

    if (kterm_starts_with(line, "size ")) {
        args = kterm_skip_cmd(line);
        kterm_cmd_size(args);
        return;
    }

    if (kterm_starts_with(line, "read ")) {
        args = kterm_skip_cmd(line);
        if (args[0]) {
            const char* data = kfs_read(args);
            if (data) {
                serial_print(data);
                serial_print("\r\n");
            } else {
                serial_print("not found\r\n");
            }
        } else {
            serial_print("usage: read <name>\r\n");
        }
        return;
    }

    if (kterm_starts_with(line, "rm ")) {
        args = kterm_skip_cmd(line);
        if (args[0] && kfs_delete(args)) {
            serial_print("ok\r\n");
        } else {
            serial_print("fail\r\n");
        }
        return;
    }

    if (kterm_starts_with(line, "write ")) {
        args = kterm_skip_cmd(line);
        kterm_cmd_write(args);
        return;
    }

    if (kterm_starts_with(line, "append ")) {
        args = kterm_skip_cmd(line);
        kterm_cmd_append(args);
        return;
    }

    if (kterm_streq(line, "exit")) {
        *keep_running = 0;
        serial_print("bye\r\n");
        return;
    }

    if (klua_cmd_run(line)) {
        serial_print("lua command executed\r\n");
        return;
    }

    serial_print("unknown command\r\n");
}

void kterm_run(void) {
    char line[KTERM_BUF_SIZE];
    size_t pos = 0;
    int keep_running = 1;

    serial_init();

    kio_writeln("[kterm] serial terminal aberto em COM1");
    serial_print("\r\n[kterm] serial terminal ready\r\n");
    kterm_print_help();
    kterm_print_prompt();

    while (keep_running) {
        char c = serial_getchar();

        if (c == '\r' || c == '\n') {
            serial_print("\r\n");
            line[pos] = 0;
            kterm_exec(line, &keep_running);
            pos = 0;
            if (keep_running) {
                kterm_print_prompt();
            }
            continue;
        }

        if ((c == 8 || c == 127) && pos > 0) {
            pos--;
            serial_print("\b \b");
            continue;
        }

        if (c >= 32 && c <= 126) {
            if (pos + 1 >= KTERM_BUF_SIZE) {
                serial_print("\r\n[error] line too long\r\n");
                pos = 0;
                kterm_print_prompt();
                continue;
            }
            line[pos++] = c;
            serial_putchar(c);
        }
    }
}
