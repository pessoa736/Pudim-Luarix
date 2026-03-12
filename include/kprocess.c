#include "kprocess.h"
#include "kheap.h"
#include "serial.h"
#include "arch.h"
#include "kdisplay.h"

#if ARCH_HAS_VGA
#include "vga.h"
#endif

#define KPROCESS_THREAD_TABLE "__kproc_threads"
#define KPROCESS_PT_ENTRIES 512u
#define KPROCESS_KERNEL_PML4_ADDR 0x200000ull
#define KPROCESS_KERNEL_PDPT_ADDR 0x201000ull
#define KPROCESS_KERNEL_PD_ADDR 0x202000ull
#define KPROCESS_PAGE_SIZE_2M 0x200000ull
#define KPROCESS_SHARED_BYTES (64ull * 1024ull * 1024ull)
#define KPROCESS_SHARED_PD_ENTRIES ((uint32_t)(KPROCESS_SHARED_BYTES / KPROCESS_PAGE_SIZE_2M))
#define KPROCESS_PAGE_PRESENT 0x1ull
#define KPROCESS_PAGE_RW 0x2ull
#define KPROCESS_ADDR_MASK 0x000FFFFFFFFFF000ull
#define KPROCESS_ENABLE_CR3_SWITCH 0

#ifndef KPROCESS_POLL_BUDGET
#define KPROCESS_POLL_BUDGET 4u
#endif

#ifndef KPROCESS_PREEMPT_INSTRUCTIONS
#define KPROCESS_PREEMPT_INSTRUCTIONS 4096u
#endif

extern uint8_t __kernel_end;

typedef struct {
    unsigned int pid;
    kprocess_state_t state;
    lua_State* thread;
    uint64_t cr3_root;
    unsigned int uid;
    unsigned int gid;
} kprocess_entry_t;

static lua_State* g_kprocess_main_lua = 0;
static kprocess_entry_t g_kprocess_entries[KPROCESS_MAX_PROCESSES];
static unsigned int g_kprocess_next_pid = 1;
static unsigned int g_kprocess_last_index = 0;
static unsigned int g_kprocess_current_pid = 0;
static volatile unsigned int g_kprocess_pending_ticks = 0;
static uint64_t g_kprocess_pml4_pool[KPROCESS_MAX_PROCESSES][KPROCESS_PT_ENTRIES] __attribute__((aligned(4096)));
static uint64_t g_kprocess_pdpt_pool[KPROCESS_MAX_PROCESSES][KPROCESS_PT_ENTRIES] __attribute__((aligned(4096)));
static uint64_t g_kprocess_pd_pool[KPROCESS_MAX_PROCESSES][KPROCESS_PT_ENTRIES] __attribute__((aligned(4096)));

static inline uint64_t kprocess_read_cr3(void) {
#ifdef ARCH_X86_64
    uint64_t value;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
#else
    return 0;
#endif
}

static inline void kprocess_write_cr3(uint64_t value) {
#ifdef ARCH_X86_64
    asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
#else
    (void)value;
#endif
}

static void kprocess_copy_page_table(uint64_t* dst, const uint64_t* src) {
    uint32_t i;

    if (!dst || !src) {
        return;
    }

    for (i = 0; i < KPROCESS_PT_ENTRIES; i++) {
        dst[i] = src[i];
    }
}

static int kprocess_streq(const char* a, const char* b) {
    uint32_t i = 0;

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

static int kprocess_setup_address_space(unsigned int index) {
    uint64_t* pml4;
    uint64_t* pdpt;
    uint64_t* pd;
    const uint64_t* kernel_pml4;
    const uint64_t* kernel_pdpt;
    const uint64_t* kernel_pd;
    uint64_t pml4_phys;
    uint64_t pdpt_phys;
    uint64_t pd_phys;

    if (index >= KPROCESS_MAX_PROCESSES) {
        return 0;
    }

    pml4 = g_kprocess_pml4_pool[index];
    pdpt = g_kprocess_pdpt_pool[index];
    pd = g_kprocess_pd_pool[index];

    kernel_pml4 = (const uint64_t*)(size_t)KPROCESS_KERNEL_PML4_ADDR;
    kernel_pdpt = (const uint64_t*)(size_t)KPROCESS_KERNEL_PDPT_ADDR;
    kernel_pd = (const uint64_t*)(size_t)KPROCESS_KERNEL_PD_ADDR;

    kprocess_copy_page_table(pml4, kernel_pml4);
    kprocess_copy_page_table(pdpt, kernel_pdpt);
    kprocess_copy_page_table(pd, kernel_pd);

    /*
     Keep only the shared kernel range mapped in process CR3 roots.
     Everything above this region is left non-present to reduce
     accidental cross-space access.
    */
    for (uint32_t i = KPROCESS_SHARED_PD_ENTRIES; i < KPROCESS_PT_ENTRIES; i++) {
        pd[i] = 0;
    }

    pml4_phys = ((uint64_t)(size_t)pml4) & KPROCESS_ADDR_MASK;
    pdpt_phys = ((uint64_t)(size_t)pdpt) & KPROCESS_ADDR_MASK;
    pd_phys = ((uint64_t)(size_t)pd) & KPROCESS_ADDR_MASK;

    pml4[0] = pdpt_phys | KPROCESS_PAGE_PRESENT | KPROCESS_PAGE_RW;
    pdpt[0] = pd_phys | KPROCESS_PAGE_PRESENT | KPROCESS_PAGE_RW;

    g_kprocess_entries[index].cr3_root = pml4_phys;
    return g_kprocess_entries[index].cr3_root != 0;
}

static int kprocess_setup_chunk_env(lua_State* thread, unsigned int pid) {
    const char* upname;

    if (!thread || !lua_isfunction(thread, -1)) {
        return 0;
    }

    upname = lua_getupvalue(thread, -1, 1);
    if (!upname) {
        return 1;
    }

    lua_pop(thread, 1);

    if (!kprocess_streq(upname, "_ENV")) {
        return 1;
    }

    /* Create isolated env with fallback to base globals via __index = _G. */
    lua_createtable(thread, 0, 2);
    lua_pushinteger(thread, (lua_Integer)pid);
    lua_setfield(thread, -2, "__pid");

    lua_createtable(thread, 0, 1);
    lua_getglobal(thread, "_G");
    lua_setfield(thread, -2, "__index");
    lua_setmetatable(thread, -2);

    if (!lua_setupvalue(thread, -2, 1)) {
        return 0;
    }

    return 1;
}

struct kprocess_reader_state {
    const char* code;
    size_t size;
    int done;
};

static const char* kprocess_reader(lua_State* L, void* data, size_t* size) {
    struct kprocess_reader_state* state = (struct kprocess_reader_state*)data;
    (void)L;

    if (state->done) {
        *size = 0;
        return NULL;
    }

    state->done = 1;
    *size = state->size;
    return state->code;
}

static int kprocess_ensure_thread_table(void) {
    if (!g_kprocess_main_lua) {
        return 0;
    }

    lua_getglobal(g_kprocess_main_lua, KPROCESS_THREAD_TABLE);
    if (!lua_istable(g_kprocess_main_lua, -1)) {
        lua_pop(g_kprocess_main_lua, 1);
        lua_createtable(g_kprocess_main_lua, 0, KPROCESS_MAX_PROCESSES);
        lua_setglobal(g_kprocess_main_lua, KPROCESS_THREAD_TABLE);
        lua_getglobal(g_kprocess_main_lua, KPROCESS_THREAD_TABLE);
    }

    return lua_istable(g_kprocess_main_lua, -1);
}

static void kprocess_strcopy(char* dst, unsigned int dst_size, const char* src) {
    unsigned int i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && (i + 1) < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static void kprocess_u32_to_str(unsigned int value, char* out, unsigned int out_size) {
    char tmp[16];
    unsigned int n = 0;
    unsigned int p = 0;

    if (!out || out_size < 2) {
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

    while (n > 0 && p + 1 < out_size) {
        out[p++] = tmp[--n];
    }

    out[p] = 0;
}

static unsigned int kprocess_strlen_bounded(const char* s, unsigned int max_len_plus_one) {
    unsigned int n = 0;

    if (!s) {
        return 0;
    }

    while (s[n] && n < max_len_plus_one) {
        n++;
    }

    return n;
}

static int kprocess_find_slot_by_pid(unsigned int pid) {
    unsigned int i;

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        if (g_kprocess_entries[i].pid == pid && g_kprocess_entries[i].state != KPROCESS_STATE_DEAD) {
            return (int)i;
        }
    }

    return -1;
}

static int kprocess_find_slot_by_state_not_dead(void) {
    unsigned int i;

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        if (g_kprocess_entries[i].state == KPROCESS_STATE_DEAD) {
            return (int)i;
        }
    }

    return -1;
}

static void kprocess_release_slot(unsigned int index) {
    if (index >= KPROCESS_MAX_PROCESSES) {
        return;
    }

    if (g_kprocess_main_lua && g_kprocess_entries[index].pid != 0) {
        if (kprocess_ensure_thread_table()) {
            lua_pushnil(g_kprocess_main_lua);
            lua_rawseti(g_kprocess_main_lua, -2, (lua_Integer)g_kprocess_entries[index].pid);
            lua_pop(g_kprocess_main_lua, 1);
        }
    }

    g_kprocess_entries[index].pid = 0;
    g_kprocess_entries[index].state = KPROCESS_STATE_DEAD;
    g_kprocess_entries[index].thread = 0;
    g_kprocess_entries[index].cr3_root = 0;
    g_kprocess_entries[index].uid = 0;
    g_kprocess_entries[index].gid = 0;
}

static void kprocess_print_lua_traceback(lua_State* L) {
    lua_Debug ar;
    int level;

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
    }
#endif

    kdisplay_write("  traceback:\n");
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print("  traceback:\r\n");
    }

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }
#endif

    for (level = 0; level < 16; level++) {
        if (!lua_getstack(L, level, &ar)) {
            break;
        }

        lua_getinfo(L, "Snl", &ar);

        kdisplay_write("    ");
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            serial_print("    ");
        }

        if (ar.source && ar.source[0]) {
            kdisplay_write(ar.source);
            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print(ar.source);
            }
        }

        if (ar.currentline > 0) {
            char linebuf[16];
            kprocess_u32_to_str((unsigned int)ar.currentline, linebuf, sizeof(linebuf));
            kdisplay_write(":");
            kdisplay_write(linebuf);
            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print(":");
                serial_print(linebuf);
            }
        }

        kdisplay_write(": ");
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            serial_print(": ");
        }

        if (ar.name && ar.name[0]) {
            kdisplay_write("in '");
            kdisplay_write(ar.name);
            kdisplay_write("'");
            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print("in '");
                serial_print(ar.name);
                serial_print("'");
            }
        } else if (ar.what && ar.what[0]) {
            kdisplay_write("in ");
            kdisplay_write(ar.what);
            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print("in ");
                serial_print(ar.what);
            }
        }

        kdisplay_write("\n");
        if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
            serial_print("\r\n");
        }
    }
}

static void kprocess_set_error(unsigned int index, const char* prefix, const char* msg) {
    char line[192];

    if (index >= KPROCESS_MAX_PROCESSES) {
        return;
    }

    line[0] = 0;
    kprocess_strcopy(line, sizeof(line), prefix);

    if (msg && line[0]) {
        unsigned int i = 0;
        while (line[i] && i + 1 < sizeof(line)) {
            i++;
        }

        if (i + 1 < sizeof(line)) {
            line[i++] = ' ';
        }

        while (*msg && i + 1 < sizeof(line)) {
            line[i++] = *msg++;
        }
        line[i] = 0;
    }

    kdisplay_write(line);
    kdisplay_write("\n");
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        serial_print(line);
        serial_print("\r\n");
    }
    g_kprocess_entries[index].state = KPROCESS_STATE_ERROR;
}

int kprocess_init(lua_State* main_lua) {
    unsigned int i;
    uint64_t kernel_end_addr;

    if (!main_lua) {
        return 0;
    }

    kernel_end_addr = (uint64_t)(size_t)&__kernel_end;
    if (kernel_end_addr >= KPROCESS_SHARED_BYTES) {
        return 0;
    }

    g_kprocess_main_lua = main_lua;
    g_kprocess_next_pid = 1;
    g_kprocess_last_index = 0;
    g_kprocess_current_pid = 0;
    g_kprocess_pending_ticks = 0;

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        g_kprocess_entries[i].pid = 0;
        g_kprocess_entries[i].state = KPROCESS_STATE_DEAD;
        g_kprocess_entries[i].thread = 0;
        g_kprocess_entries[i].cr3_root = 0;
        g_kprocess_entries[i].uid = 0;
        g_kprocess_entries[i].gid = 0;
    }

    if (!kprocess_ensure_thread_table()) {
        return 0;
    }
    lua_pop(g_kprocess_main_lua, 1);

    return 1;
}

int kprocess_create(const char* script) {
    int slot;
    lua_State* thread;
    int load_rc;
    unsigned int script_len;
    struct kprocess_reader_state reader_state;

    if (!g_kprocess_main_lua || !script || !script[0]) {
        return 0;
    }

    script_len = kprocess_strlen_bounded(script, KPROCESS_MAX_SCRIPT_SIZE + 1);
    if (script_len == 0 || script_len > KPROCESS_MAX_SCRIPT_SIZE) {
        return 0;
    }

    slot = kprocess_find_slot_by_state_not_dead();
    if (slot < 0) {
        return 0;
    }

    thread = lua_newthread(g_kprocess_main_lua);
    if (!thread) {
        lua_pop(g_kprocess_main_lua, 1);
        return 0;
    }

    reader_state.code = script;
    reader_state.size = script_len;
    reader_state.done = 0;

    load_rc = lua_load(thread, kprocess_reader, &reader_state, "process_code", "t");
    if (load_rc != LUA_OK) {
        const char* err = lua_tostring(thread, -1);
        if (err) {
            kdisplay_write("process load error: ");
            kdisplay_write(err);
            kdisplay_write("\n");
            if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
                serial_print("process load error: ");
                serial_print(err);
                serial_print("\r\n");
            }
        }

        lua_pop(thread, 1);
        lua_pop(g_kprocess_main_lua, 1);
        return 0;
    }

    g_kprocess_entries[slot].pid = g_kprocess_next_pid++;
    g_kprocess_entries[slot].state = KPROCESS_STATE_READY;
    g_kprocess_entries[slot].thread = thread;
    g_kprocess_entries[slot].cr3_root = 0;

    if (!kprocess_setup_address_space((unsigned int)slot)) {
        lua_settop(thread, 0);
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    if (!kprocess_setup_chunk_env(thread, g_kprocess_entries[slot].pid)) {
        lua_settop(thread, 0);
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    if (!kprocess_ensure_thread_table()) {
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    lua_pushvalue(g_kprocess_main_lua, -2);
    lua_rawseti(g_kprocess_main_lua, -2, (lua_Integer)g_kprocess_entries[slot].pid);

    lua_pop(g_kprocess_main_lua, 2);

    return (int)g_kprocess_entries[slot].pid;
}

int kprocess_create_function(lua_State* owner, int func_index) {
    int slot;
    lua_State* thread;
    int abs_index;

    if (!g_kprocess_main_lua || !owner) {
        return 0;
    }

    if (owner != g_kprocess_main_lua) {
        return 0;
    }

    abs_index = lua_absindex(owner, func_index);
    if (!lua_isfunction(owner, abs_index)) {
        return 0;
    }

    slot = kprocess_find_slot_by_state_not_dead();
    if (slot < 0) {
        return 0;
    }

    thread = lua_newthread(g_kprocess_main_lua);
    if (!thread) {
        lua_pop(g_kprocess_main_lua, 1);
        return 0;
    }

    lua_pushvalue(owner, abs_index);
    lua_xmove(owner, thread, 1);

    if (!lua_isfunction(thread, -1)) {
        lua_settop(thread, 0);
        lua_pop(g_kprocess_main_lua, 1);
        return 0;
    }

    g_kprocess_entries[slot].pid = g_kprocess_next_pid++;
    g_kprocess_entries[slot].state = KPROCESS_STATE_READY;
    g_kprocess_entries[slot].thread = thread;
    g_kprocess_entries[slot].cr3_root = 0;

    if (!kprocess_setup_address_space((unsigned int)slot)) {
        lua_settop(thread, 0);
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    if (!kprocess_setup_chunk_env(thread, g_kprocess_entries[slot].pid)) {
        lua_settop(thread, 0);
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    if (!kprocess_ensure_thread_table()) {
        lua_pop(g_kprocess_main_lua, 1);
        kprocess_release_slot((unsigned int)slot);
        return 0;
    }

    lua_pushvalue(g_kprocess_main_lua, -2);
    lua_rawseti(g_kprocess_main_lua, -2, (lua_Integer)g_kprocess_entries[slot].pid);

    lua_pop(g_kprocess_main_lua, 2);

    return (int)g_kprocess_entries[slot].pid;
}

int kprocess_kill(unsigned int pid) {
    int slot;

    if (pid == 0) {
        return 0;
    }

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    kprocess_release_slot((unsigned int)slot);
    return 1;
}

unsigned int kprocess_current_pid(lua_State* L) {
    unsigned int i;

    if (g_kprocess_current_pid != 0) {
        return g_kprocess_current_pid;
    }

    if (!L) {
        return 0;
    }

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        if (g_kprocess_entries[i].state != KPROCESS_STATE_DEAD && g_kprocess_entries[i].thread == L) {
            return g_kprocess_entries[i].pid;
        }
    }

    return 0;
}

unsigned int kprocess_count(void) {
    unsigned int i;
    unsigned int total = 0;

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        if (g_kprocess_entries[i].state != KPROCESS_STATE_DEAD) {
            total++;
        }
    }

    return total;
}

unsigned int kprocess_max(void) {
    return KPROCESS_MAX_PROCESSES;
}

unsigned int kprocess_pid_at(unsigned int index) {
    unsigned int i;
    unsigned int seen = 0;

    for (i = 0; i < KPROCESS_MAX_PROCESSES; i++) {
        if (g_kprocess_entries[i].state != KPROCESS_STATE_DEAD) {
            if (seen == index) {
                return g_kprocess_entries[i].pid;
            }
            seen++;
        }
    }

    return 0;
}

kprocess_state_t kprocess_state_of(unsigned int pid) {
    int slot;

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return KPROCESS_STATE_DEAD;
    }

    return g_kprocess_entries[slot].state;
}

static void kprocess_preempt_hook(lua_State* L, lua_Debug* ar) {
    (void)ar;
    lua_yield(L, 0);
}

int kprocess_tick(void) {
    unsigned int scanned = 0;
    unsigned int index = g_kprocess_last_index;

    if (!g_kprocess_main_lua) {
        return 0;
    }

    while (scanned < KPROCESS_MAX_PROCESSES) {
        int rc;
        int nresults = 0;
        kprocess_entry_t* entry;

        if (index >= KPROCESS_MAX_PROCESSES) {
            index = 0;
        }

        entry = &g_kprocess_entries[index];
        if (entry->state == KPROCESS_STATE_READY && entry->thread) {
            uint64_t old_cr3;

            entry->state = KPROCESS_STATE_RUNNING;
            g_kprocess_current_pid = entry->pid;

            old_cr3 = kprocess_read_cr3();
#if KPROCESS_ENABLE_CR3_SWITCH
            if (entry->cr3_root != 0) {
                kprocess_write_cr3(entry->cr3_root);
            }
#endif

            lua_sethook(entry->thread, kprocess_preempt_hook,
                        LUA_MASKCOUNT, KPROCESS_PREEMPT_INSTRUCTIONS);
            rc = lua_resume(entry->thread, NULL, 0, &nresults);
            lua_sethook(entry->thread, NULL, 0, 0);

#if KPROCESS_ENABLE_CR3_SWITCH
            if (entry->cr3_root != 0) {
                kprocess_write_cr3(old_cr3);
            }
#endif

            (void)old_cr3;
            g_kprocess_current_pid = 0;

            if (rc == LUA_OK) {
                kprocess_release_slot(index);
            } else if (rc == LUA_YIELD) {
                entry->state = KPROCESS_STATE_READY;
            } else {
                const char* err = lua_tostring(entry->thread, -1);
                kprocess_set_error(index, "process runtime error:", err ? err : "(unknown)");
                kprocess_print_lua_traceback(entry->thread);
                lua_pop(entry->thread, 1);
                kprocess_release_slot(index);
            }

            g_kprocess_last_index = index + 1;
            return 1;
        }

        index++;
        scanned++;
    }

    return 0;
}

void kprocess_request_tick(void) {
    if (g_kprocess_pending_ticks < 1024u) {
        g_kprocess_pending_ticks++;
    }
}

int kprocess_poll(void) {
    unsigned int budget = KPROCESS_POLL_BUDGET;
    int ran_any = 0;

    while (budget > 0 && g_kprocess_pending_ticks > 0) {
        g_kprocess_pending_ticks--;
        if (kprocess_tick()) {
            ran_any = 1;
        }
        budget--;
    }

    return ran_any;
}

unsigned int kprocess_get_uid(unsigned int pid) {
    int slot;

    if (pid == 0) {
        return 0;
    }

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    return g_kprocess_entries[slot].uid;
}

unsigned int kprocess_get_gid(unsigned int pid) {
    int slot;

    if (pid == 0) {
        return 0;
    }

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    return g_kprocess_entries[slot].gid;
}

int kprocess_set_uid(unsigned int pid, unsigned int uid) {
    int slot;

    if (pid == 0) {
        return 0;
    }

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    g_kprocess_entries[slot].uid = uid;
    return 1;
}

int kprocess_set_gid(unsigned int pid, unsigned int gid) {
    int slot;

    if (pid == 0) {
        return 0;
    }

    slot = kprocess_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    g_kprocess_entries[slot].gid = gid;
    return 1;
}
