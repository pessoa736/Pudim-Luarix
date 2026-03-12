#include "kterm.h"
#include "kevent.h"
#include "kio.h"
#include "kfs.h"
#include "keyboard.h"
#include "APISLua/keventlua.h"
#include "klua.h"
#include "klua_cmds.h"
#include "kprocess.h"
#include "serial.h"
#include "vga.h"

#define KTERM_BUF_SIZE 256
#define KTERM_MAX_CMD_LEN 250
#define KTERM_MAX_LUA_CODE KTERM_MAX_CMD_LEN
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
    kio_write("plterm> ");
}

static void kterm_print_help(void) {
    kio_write("commands: help clear ls write read append rm count size save ps lcmds lua exit\n");
}

static void kterm_result(const char* s) {
    if (!s) {
        return;
    }

    kio_write(s);
}

static void kterm_cmd_lcmds(void) {
    unsigned int total = klua_cmd_count();

    if (total == 0) {
        kterm_result("(no lua commands)\r\n");
        return;
    }

    for (unsigned int i = 0; i < total; i++) {
        const char* name = klua_cmd_name_at(i);
        if (name) {
            kterm_result(name);
            kterm_result("\r\n");
        }
    }
}

static void kterm_cmd_ls(void) {
    size_t count = kfs_count();

    if (count == 0) {
        kterm_result("(empty)\r\n");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const char* name = kfs_name_at(i);
        if (name) {
            kterm_result(name);
            kterm_result("\r\n");
        }
    }
}

static void kterm_cmd_count(void) {
    char buf[32];

    kterm_u64_to_str(kfs_count(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");
}

static void kterm_cmd_size(const char* name) {
    char buf[32];

    if (!name || !name[0]) {
        kio_write("usage: size <name>\n");
        return;
    }

    kterm_u64_to_str(kfs_size(name), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");
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

static char* kterm_prepare_lua_code(char* code) {
    size_t len;

    if (!code || !code[0]) {
        return 0;
    }

    len = kterm_strlen(code);
    if (len == 0 || len > KTERM_MAX_LUA_CODE) {
        return 0;
    }

    if (len >= 2) {
        char first = code[0];
        char last = code[len - 1];

        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            code[len - 1] = 0;
            code++;

            if (!code[0]) {
                return 0;
            }
        }
    }

    return code;
}

static void kterm_cmd_lua(char* args) {
    char* code = kterm_prepare_lua_code(args);

    if (!code) {
        kio_write("usage: lua <code>\n");
        return;
    }

    if (klua_cmd_run_line(code)) {
        serial_print("[kterm] lua command executed\r\n");
        return;
    }

    klua_run(code);
}

static void kterm_cmd_write(char* args) {
    char* name = args;
    char* content;
    int i = 0;

    while (args[i] && args[i] != ' ') {
        i++;
    }

    if (!args[0] || !args[i]) {
        kio_write("usage: write <name> <content>\n");
        return;
    }

    args[i] = 0;
    content = &args[i + 1];

    if (kfs_write(name, content)) {
        kterm_result("ok\r\n");
    } else {
        kterm_result("fail\r\n");
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
        kio_write("usage: append <name> <content>\n");
        return;
    }

    args[i] = 0;
    content = &args[i + 1];

    if (kfs_append(name, content)) {
        kterm_result("ok\r\n");
    } else {
        kterm_result("fail\r\n");
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

    if (kterm_streq(line, "save")) {
        if (kfs_persist_available()) {
            if (kfs_persist_save()) {
                kterm_result("saved to disk\r\n");
            } else {
                kterm_result("save failed\r\n");
            }
        } else {
            kterm_result("no storage disk\r\n");
        }
        return;
    }

    if (kterm_streq(line, "ps")) {
        unsigned int proc_count = kprocess_count();
        if (proc_count == 0) {
            kterm_result("(no processes)\r\n");
        } else {
            unsigned int pi;
            for (pi = 0; pi < proc_count; pi++) {
                unsigned int ppid = kprocess_pid_at(pi);
                kprocess_state_t pstate;
                const char* sname = "dead";
                char pidbuf[16];
                if (ppid == 0) {
                    continue;
                }
                pstate = kprocess_state_of(ppid);
                if (pstate == KPROCESS_STATE_READY) {
                    sname = "ready";
                } else if (pstate == KPROCESS_STATE_RUNNING) {
                    sname = "running";
                } else if (pstate == KPROCESS_STATE_ERROR) {
                    sname = "error";
                }
                kterm_u64_to_str(ppid, pidbuf, sizeof(pidbuf));
                kterm_result("pid ");
                kterm_result(pidbuf);
                kterm_result("  ");
                kterm_result(sname);
                kterm_result("\r\n");
            }
        }
        return;
    }

    if (kterm_streq(line, "lua")) {
        kio_write("usage: lua <code>\n");
        return;
    }

    if (kterm_starts_with(line, "lua ")) {
        args = kterm_skip_cmd(line);
        kterm_cmd_lua(args);
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
                kterm_result(data);
                kterm_result("\r\n");
            } else {
                kterm_result("not found\r\n");
            }
        } else {
            kio_write("usage: read <name>\n");
        }
        return;
    }

    if (kterm_starts_with(line, "rm ")) {
        args = kterm_skip_cmd(line);
        if (args[0] && kfs_delete(args)) {
            kterm_result("ok\r\n");
        } else {
            kterm_result("fail\r\n");
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
        kterm_result("bye\r\n");
        return;
    }

    if (klua_cmd_run_line(line)) {
        serial_print("[kterm] lua command executed\r\n");
        return;
    }

    kio_write("unknown command\n");
}

void kterm_run(void) {
    char line[KTERM_BUF_SIZE];
    size_t len = 0;
    size_t cur = 0;
    int keep_running = 1;
    uint16_t line_start_cursor;

    serial_init();

    kio_writeln("terminal na tela (VGA), entrada via teclado/COM1");
    serial_print("\r\nserial debug ready (COM1)\r\n");
    kterm_print_help();
    kterm_print_prompt();
    line_start_cursor = vga_get_cursor();

    while (keep_running) {
        unsigned char key = 0;

        kprocess_poll();
        keventlua_dispatch(klua_state());

        if (!keyboard_getkey_nonblock(&key)) {
            continue;
        }

        /* Push key_pressed event for all keys */
        {
            char kbuf[2];
            kbuf[0] = (char)key;
            kbuf[1] = 0;
            kevent_push_string("key_pressed", kbuf);
        }

        /* Enter */
        if (key == '\r' || key == '\n') {
            vga_putchar('\n');
            line[len] = 0;
            kterm_exec(line, &keep_running);
            len = 0;
            cur = 0;
            if (keep_running) {
                kio_write("\n");
                kterm_print_prompt();
                line_start_cursor = vga_get_cursor();
            }
            continue;
        }

        /* Backspace */
        if ((key == 8 || key == 127) && cur > 0) {
            size_t i;
            cur--;
            for (i = cur; i < len - 1; i++) {
                line[i] = line[i + 1];
            }
            len--;

            /* Redraw from cursor position */
            vga_set_cursor(line_start_cursor + (uint16_t)cur);
            for (i = cur; i < len; i++) {
                vga_put_at(line_start_cursor + (uint16_t)i, line[i]);
            }
            vga_put_at(line_start_cursor + (uint16_t)len, ' ');
            vga_set_cursor(line_start_cursor + (uint16_t)cur);
            continue;
        }

        /* Left arrow */
        if (key == KBD_KEY_LEFT) {
            if (cur > 0) {
                cur--;
                vga_set_cursor(line_start_cursor + (uint16_t)cur);
            }
            continue;
        }

        /* Right arrow */
        if (key == KBD_KEY_RIGHT) {
            if (cur < len) {
                cur++;
                vga_set_cursor(line_start_cursor + (uint16_t)cur);
            }
            continue;
        }

        /* Home */
        if (key == KBD_KEY_HOME) {
            cur = 0;
            vga_set_cursor(line_start_cursor);
            continue;
        }

        /* End */
        if (key == KBD_KEY_END) {
            cur = len;
            vga_set_cursor(line_start_cursor + (uint16_t)len);
            continue;
        }

        /* Delete */
        if (key == KBD_KEY_DELETE) {
            if (cur < len) {
                size_t i;
                for (i = cur; i < len - 1; i++) {
                    line[i] = line[i + 1];
                }
                len--;
                for (i = cur; i < len; i++) {
                    vga_put_at(line_start_cursor + (uint16_t)i, line[i]);
                }
                vga_put_at(line_start_cursor + (uint16_t)len, ' ');
                vga_set_cursor(line_start_cursor + (uint16_t)cur);
            }
            continue;
        }

        /* Ignore other special keys */
        if (key >= 0x80) {
            continue;
        }

        /* Printable character */
        if (key >= 32 && key <= 126) {
            size_t i;

            if (len + 1 >= KTERM_BUF_SIZE) {
                kio_write("\nerror: line too long\n");
                len = 0;
                cur = 0;
                kterm_print_prompt();
                line_start_cursor = vga_get_cursor();
                continue;
            }

            /* Shift chars right to insert at cursor */
            for (i = len; i > cur; i--) {
                line[i] = line[i - 1];
            }
            line[cur] = (char)key;
            len++;
            cur++;

            /* Redraw from insert position */
            for (i = cur - 1; i < len; i++) {
                vga_put_at(line_start_cursor + (uint16_t)i, line[i]);
            }
            vga_set_cursor(line_start_cursor + (uint16_t)cur);
        }
    }
}
