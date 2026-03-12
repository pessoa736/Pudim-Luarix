#include "kterm.h"
#include "kevent.h"
#include "kio.h"
#include "kfs.h"
#include "keyboard.h"
#include "mouse.h"
#include "APISLua/keventlua.h"
#include "klua.h"
#include "klua_cmds.h"
#include "kprocess.h"
#include "ksecurity.h"
#include "kdebug.h"
#include "kheap.h"
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
    kio_write("\033[32mplterm>\033[0m ");
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

static void kterm_mode_to_str(unsigned int mode, char* out) {
    const char* rwx = "rwx";
    unsigned int i;

    for (i = 0; i < 9; i++) {
        unsigned int bit = (mode >> (8 - i)) & 1;
        out[i] = bit ? rwx[i % 3] : '-';
    }

    out[9] = 0;
}

/*
 * MD — Menu de Despensa (Pantry Menu)
 *
 * Like peeking into the despensa to see which pudding
 * ingredients are shelved. The plain look just reads
 * the labels; -l inspects every jar in detail;
 * -s checks weights; -p reads the recipe permissions.
 */
#define KTERM_MD_NAMES 0
#define KTERM_MD_LONG  1
#define KTERM_MD_SIZE  2
#define KTERM_MD_PERM  3

static void kterm_cmd_md(int mode) {
    size_t count = kfs_count();
    size_t i;

    if (count == 0) {
        kterm_result("(empty)\r\n");
        return;
    }

    for (i = 0; i < count; i++) {
        const char* name = kfs_name_at(i);
        if (!name) {
            continue;
        }

        if (mode == KTERM_MD_LONG) {
            char modebuf[16];
            char uidbuf[16];
            char gidbuf[16];
            char sizebuf[16];

            kterm_mode_to_str(kfs_get_mode(name), modebuf);
            kterm_u64_to_str(kfs_get_owner(name), uidbuf, sizeof(uidbuf));
            kterm_u64_to_str(kfs_get_group(name), gidbuf, sizeof(gidbuf));
            kterm_u64_to_str(kfs_size(name), sizebuf, sizeof(sizebuf));

            kterm_result(modebuf);
            kterm_result("  ");
            kterm_result(uidbuf);
            kterm_result(" ");
            kterm_result(gidbuf);
            kterm_result("  ");
            kterm_result(sizebuf);
            kterm_result("  ");
            kterm_result(name);
        } else if (mode == KTERM_MD_SIZE) {
            char sizebuf[16];
            kterm_u64_to_str(kfs_size(name), sizebuf, sizeof(sizebuf));
            kterm_result(sizebuf);
            kterm_result("  ");
            kterm_result(name);
        } else if (mode == KTERM_MD_PERM) {
            char modebuf[16];
            kterm_mode_to_str(kfs_get_mode(name), modebuf);
            kterm_result(modebuf);
            kterm_result("  ");
            kterm_result(name);
        } else {
            kterm_result(name);
        }

        kterm_result("\r\n");
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

/* --- Command handlers (each receives args after command name) --- */

static void kterm_handle_clear(char* args, int* kr) {
    (void)args; (void)kr;
    kio_clear();
}

static void kterm_handle_md(char* args, int* kr) {
    (void)kr;
    int mode = KTERM_MD_NAMES;

    if (args && args[0] == '-') {
        if (args[1] == 'l' && (args[2] == 0 || args[2] == ' ')) {
            mode = KTERM_MD_LONG;
        } else if (args[1] == 's' && (args[2] == 0 || args[2] == ' ')) {
            mode = KTERM_MD_SIZE;
        } else if (args[1] == 'p' && (args[2] == 0 || args[2] == ' ')) {
            mode = KTERM_MD_PERM;
        } else {
            kterm_result("usage: md [-l|-s|-p]\r\n");
            return;
        }
    }

    kterm_cmd_md(mode);
}

static void kterm_handle_count(char* args, int* kr) {
    (void)args; (void)kr;
    kterm_cmd_count();
}

static void kterm_handle_lcmds(char* args, int* kr) {
    (void)args; (void)kr;
    kterm_cmd_lcmds();
}

static void kterm_handle_save(char* args, int* kr) {
    (void)args; (void)kr;
    if (kfs_persist_available()) {
        if (kfs_persist_save()) {
            kterm_result("saved to disk\r\n");
        } else {
            kterm_result("save failed\r\n");
        }
    } else {
        kterm_result("no storage disk\r\n");
    }
}

static void kterm_handle_ps(char* args, int* kr) {
    unsigned int proc_count;
    (void)args; (void)kr;

    proc_count = kprocess_count();
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
}

static void kterm_handle_lua(char* args, int* kr) {
    (void)kr;
    kterm_cmd_lua(args);
}

static void kterm_handle_size(char* args, int* kr) {
    (void)kr;
    kterm_cmd_size(args);
}

static void kterm_handle_read(char* args, int* kr) {
    (void)kr;
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
}

static void kterm_handle_rm(char* args, int* kr) {
    (void)kr;
    if (args[0] && kfs_delete(args)) {
        kterm_result("ok\r\n");
    } else {
        kterm_result("fail\r\n");
    }
}

static void kterm_handle_write(char* args, int* kr) {
    (void)kr;
    kterm_cmd_write(args);
}

static void kterm_handle_append(char* args, int* kr) {
    (void)kr;
    kterm_cmd_append(args);
}

static void kterm_handle_exit(char* args, int* kr) {
    (void)args;
    *kr = 0;
    kterm_result("bye\r\n");
}

static void kterm_handle_whoami(char* args, int* kr) {
    char name[KSECURITY_MAX_USERNAME];
    (void)args; (void)kr;

    /* Terminal runs as root (uid 0) */
    if (ksecurity_get_user_name(0, name, sizeof(name))) {
        kterm_result(name);
    } else {
        kterm_result("uid 0");
    }
    kterm_result("\r\n");
}

static void kterm_handle_chmod(char* args, int* kr) {
    char name_buf[48];
    unsigned int mode = 0;
    int i = 0;
    int ni = 0;
    (void)kr;

    if (!args || !args[0]) {
        kio_write("usage: chmod <name> <mode>\n");
        return;
    }

    /* Parse filename */
    while (args[i] && args[i] != ' ' && ni + 1 < (int)sizeof(name_buf)) {
        name_buf[ni++] = args[i++];
    }
    name_buf[ni] = 0;

    while (args[i] == ' ') i++;

    if (!args[i]) {
        kio_write("usage: chmod <name> <mode>\n");
        return;
    }

    /* Parse octal mode */
    while (args[i] >= '0' && args[i] <= '7') {
        mode = mode * 8 + (unsigned int)(args[i] - '0');
        i++;
    }

    if (mode > 0777u) {
        kterm_result("invalid mode (use octal 0-777)\r\n");
        return;
    }

    if (kfs_chmod(name_buf, mode)) {
        kterm_result("ok\r\n");
    } else {
        kterm_result("fail\r\n");
    }
}

static void kterm_handle_chown(char* args, int* kr) {
    char name_buf[48];
    unsigned int uid = 0;
    unsigned int gid = 0;
    int i = 0;
    int ni = 0;
    (void)kr;

    if (!args || !args[0]) {
        kio_write("usage: chown <name> <uid> <gid>\n");
        return;
    }

    while (args[i] && args[i] != ' ' && ni + 1 < (int)sizeof(name_buf)) {
        name_buf[ni++] = args[i++];
    }
    name_buf[ni] = 0;

    while (args[i] == ' ') i++;
    while (args[i] >= '0' && args[i] <= '9') {
        uid = uid * 10 + (unsigned int)(args[i] - '0');
        i++;
    }

    while (args[i] == ' ') i++;
    while (args[i] >= '0' && args[i] <= '9') {
        gid = gid * 10 + (unsigned int)(args[i] - '0');
        i++;
    }

    if (kfs_chown(name_buf, uid, gid)) {
        kterm_result("ok\r\n");
    } else {
        kterm_result("fail\r\n");
    }
}

static void kterm_handle_history(char* args, int* kr) {
    unsigned int count = kdebug_history_count();
    unsigned int i;
    (void)args; (void)kr;

    if (count == 0) {
        kterm_result("(no pudding layers yet)\r\n");
        return;
    }

    /* Show layers from oldest to newest, like slicing a pudding bottom-up */
    for (i = count; i > 0; i--) {
        const char* entry = kdebug_history_at(i - 1);
        if (entry) {
            char numbuf[16];
            kterm_u64_to_str(count - i + 1, numbuf, sizeof(numbuf));
            kterm_result("  ");
            kterm_result(numbuf);
            kterm_result(") ");
            kterm_result(entry);
            kterm_result("\r\n");
        }
    }
}

static void kterm_handle_inspect(char* args, int* kr) {
    char buf[32];
    (void)args; (void)kr;

    /* Ingredient check: show heap block stats */
    kterm_result("heap ingredient check:\r\n");

    kterm_result("  blocks:       ");
    kterm_u64_to_str(kdebug_heap_block_count(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");

    kterm_result("  free blocks:  ");
    kterm_u64_to_str(kdebug_heap_free_block_count(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");

    kterm_result("  largest free: ");
    kterm_u64_to_str(kdebug_heap_largest_free(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");

    kterm_result("  free bytes:   ");
    kterm_u64_to_str(kheap_free_bytes(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");

    kterm_result("  total bytes:  ");
    kterm_u64_to_str(kheap_total_bytes(), buf, sizeof(buf));
    kterm_result(buf);
    kterm_result("\r\n");
}

static void kterm_handle_getperm(char* args, int* kr) {
    char modebuf[16];
    char uidbuf[16];
    char gidbuf[16];
    (void)kr;

    if (!args || !args[0]) {
        kio_write("usage: getperm <name>\n");
        return;
    }

    if (!kfs_exists(args)) {
        kterm_result("not found\r\n");
        return;
    }

    kterm_mode_to_str(kfs_get_mode(args), modebuf);
    kterm_u64_to_str(kfs_get_owner(args), uidbuf, sizeof(uidbuf));
    kterm_u64_to_str(kfs_get_group(args), gidbuf, sizeof(gidbuf));

    kterm_result("mode: ");
    kterm_result(modebuf);
    kterm_result("  uid: ");
    kterm_result(uidbuf);
    kterm_result("  gid: ");
    kterm_result(gidbuf);
    kterm_result("\r\n");
}

/* --- Command dispatch table --- */

typedef void (*kterm_handler_t)(char* args, int* keep_running);

typedef struct {
    const char* name;
    kterm_handler_t handler;
    int needs_args;
    const char* usage;
} kterm_cmd_entry_t;

static const kterm_cmd_entry_t g_kterm_commands[] = {
    { "help",    0,                      0, 0 },
    { "clear",   kterm_handle_clear,      0, 0 },
    { "md",      kterm_handle_md,         0, "md [-l|-s|-p]" },
    { "ps",      kterm_handle_ps,         0, 0 },
    { "count",   kterm_handle_count,      0, 0 },
    { "lcmds",   kterm_handle_lcmds,      0, 0 },
    { "save",    kterm_handle_save,       0, 0 },
    { "whoami",  kterm_handle_whoami,     0, 0 },
    { "write",   kterm_handle_write,      1, "write <name> <content>" },
    { "read",    kterm_handle_read,       1, "read <name>" },
    { "append",  kterm_handle_append,     1, "append <name> <content>" },
    { "rm",      kterm_handle_rm,         1, "rm <name>" },
    { "size",    kterm_handle_size,       1, "size <name>" },
    { "lua",     kterm_handle_lua,        1, "lua <code>" },
    { "chmod",   kterm_handle_chmod,      1, "chmod <name> <mode>" },
    { "chown",   kterm_handle_chown,      1, "chown <name> <uid> <gid>" },
    { "getperm", kterm_handle_getperm,    1, "getperm <name>" },
    { "history", kterm_handle_history,    0, 0 },
    { "inspect", kterm_handle_inspect,    0, 0 },
    { "exit",    kterm_handle_exit,       0, 0 },
};

#define KTERM_CMD_COUNT (sizeof(g_kterm_commands) / sizeof(g_kterm_commands[0]))

static void kterm_print_help(void) {
    unsigned int i;
    unsigned int lua_count;

    kio_write("commands:\r\n");
    for (i = 0; i < KTERM_CMD_COUNT; i++) {
        kterm_result("  ");
        kterm_result(g_kterm_commands[i].name);
        if (g_kterm_commands[i].usage) {
            kterm_result("  - ");
            kterm_result(g_kterm_commands[i].usage);
        }
        kterm_result("\r\n");
    }

    lua_count = klua_cmd_count();
    for (i = 0; i < lua_count; i++) {
        const char* name = klua_cmd_name_at(i);
        if (name) {
            kterm_result("  ");
            kterm_result(name);
            kterm_result("\r\n");
        }
    }
}

static int kterm_match_cmd(const char* line, const char* cmd) {
    int i = 0;

    while (cmd[i]) {
        if (line[i] != cmd[i]) {
            return 0;
        }
        i++;
    }

    /* Exact match or followed by space (for args) */
    return line[i] == 0 || line[i] == ' ';
}

static void kterm_exec(char* line, int* keep_running) {
    unsigned int i;

    if (!line[0]) {
        return;
    }

    for (i = 0; i < KTERM_CMD_COUNT; i++) {
        if (!kterm_match_cmd(line, g_kterm_commands[i].name)) {
            continue;
        }

        /* Special case: help has no handler, prints from table */
        if (!g_kterm_commands[i].handler) {
            kterm_print_help();
            return;
        }

        if (g_kterm_commands[i].needs_args) {
            char* args = kterm_skip_cmd(line);
            if (!args[0] && g_kterm_commands[i].usage) {
                kio_write("usage: ");
                kio_write(g_kterm_commands[i].usage);
                kio_write("\n");
                return;
            }
            g_kterm_commands[i].handler(args, keep_running);
        } else {
            g_kterm_commands[i].handler(line, keep_running);
        }

        return;
    }

    /* Fallback: try Lua custom commands */
    if (klua_cmd_run_line(line)) {
        serial_print("[kterm] lua command executed\r\n");
        return;
    }

    kio_write("unknown command\n");
}

/* Replace current line buffer with a string and redraw */
static void kterm_set_line(char* line, size_t* len, size_t* cur,
                           uint16_t start_cursor, const char* text) {
    size_t old_len = *len;
    size_t new_len = 0;
    size_t i;

    /* Copy text into line buffer */
    while (text[new_len] && new_len + 1 < KTERM_BUF_SIZE) {
        line[new_len] = text[new_len];
        new_len++;
    }
    line[new_len] = 0;

    /* Clear old chars that extend beyond new length */
    for (i = new_len; i < old_len; i++) {
        vga_put_at(start_cursor + (uint16_t)i, ' ');
    }

    /* Draw new content */
    for (i = 0; i < new_len; i++) {
        vga_put_at(start_cursor + (uint16_t)i, line[i]);
    }

    *len = new_len;
    *cur = new_len;
    vga_set_cursor(start_cursor + (uint16_t)new_len);
}

void kterm_run(void) {
    char line[KTERM_BUF_SIZE];
    char history_draft[KTERM_BUF_SIZE]; /* saved line when browsing layers */
    size_t len = 0;
    size_t cur = 0;
    int keep_running = 1;
    int history_browse = -1; /* -1 = not browsing, >=0 = layer index */
    uint16_t line_start_cursor;

    history_draft[0] = 0;

    serial_init();

    serial_print("\r\nserial debug ready (COM1)\r\n");
    kterm_print_help();
    kterm_print_prompt();
    line_start_cursor = vga_get_cursor();

    while (keep_running) {
        unsigned char key = 0;

        kprocess_poll();
        keventlua_dispatch(klua_state());

        /* Handle mouse scroll for scrollback */
        {
            int scroll = mouse_get_scroll();
            if (scroll < 0) {
                vga_scroll_view_up((-scroll) * 3);
            } else if (scroll > 0) {
                vga_scroll_view_down(scroll * 3);
            }
        }

        if (!keyboard_getkey_nonblock(&key)) {
            continue;
        }

        /* Any keypress returns to live view if scrolled back */
        if (vga_is_scrolled_back()) {
            vga_scroll_view_down(9999);
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

            /* Trim trailing whitespace */
            while (len > 0 && line[len - 1] == ' ') {
                len--;
                line[len] = 0;
            }

            /* Push to pudding layers (history) */
            kdebug_history_push(line);
            history_browse = -1;

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

        /* Up arrow — peel back pudding layers (history) */
        if (key == KBD_KEY_UP) {
            unsigned int hcount = kdebug_history_count();
            if (hcount > 0) {
                if (history_browse < 0) {
                    /* Save current draft before browsing */
                    size_t di = 0;
                    while (di < len && di + 1 < KTERM_BUF_SIZE) {
                        history_draft[di] = line[di];
                        di++;
                    }
                    history_draft[di] = 0;
                    history_browse = 0;
                } else if ((unsigned int)history_browse + 1 < hcount) {
                    history_browse++;
                }
                {
                    const char* entry = kdebug_history_at((unsigned int)history_browse);
                    if (entry) {
                        kterm_set_line(line, &len, &cur, line_start_cursor, entry);
                    }
                }
            }
            continue;
        }

        /* Down arrow — move forward through pudding layers */
        if (key == KBD_KEY_DOWN) {
            if (history_browse >= 0) {
                history_browse--;
                if (history_browse < 0) {
                    /* Restore draft */
                    kterm_set_line(line, &len, &cur, line_start_cursor, history_draft);
                } else {
                    const char* entry = kdebug_history_at((unsigned int)history_browse);
                    if (entry) {
                        kterm_set_line(line, &len, &cur, line_start_cursor, entry);
                    }
                }
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
