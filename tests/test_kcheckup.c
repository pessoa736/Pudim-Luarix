/* Unit tests for kcheckup.c — boot taste test */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/* Block real lua.h — provide minimal Lua stubs instead */
#define lua_h
typedef struct lua_State lua_State;
#define LUA_API extern
#define lua_pop(L,n) lua_settop(L, -(n)-1)

/* Lua stub state */
static int g_lua_valid = 0;

/* Lua function stubs matching real signatures */
int lua_getglobal(lua_State* L, const char* name) { (void)L; (void)name; return 0; }
int lua_isnil(lua_State* L, int idx) { (void)L; (void)idx; return g_lua_valid ? 0 : 1; }
int lua_type(lua_State* L, int idx) { (void)L; (void)idx; return g_lua_valid ? 6 : 0; }
void lua_settop(lua_State* L, int idx) { (void)L; (void)idx; }

lua_State* klua_state(void) { return g_lua_valid ? (lua_State*)1 : (lua_State*)0; }

/* Provide __kernel_end for kheap */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* --- Stubs --- */

/* Stub serial */
void serial_print(const char* s) { (void)s; }
void serial_putchar(char c) { (void)c; }

/* Stub VGA */
void vga_print(const char* s) { (void)s; }
void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }

/* Stub kbootlog */
void kbootlog_title(const char* t) { (void)t; }
void kbootlog_write(const char* m) { (void)m; }
void kbootlog_writeln(const char* m) { (void)m; }
void kbootlog_line(const char* t, const char* m) { (void)t; (void)m; }

/* Stub ATA */
static int g_ata_available = 0;
int ata_init(void) { return g_ata_available; }
int ata_is_available(void) { return g_ata_available; }
int ata_read_sectors(uint32_t lba, uint32_t c, void* b, uint32_t bs) {
    (void)lba; (void)c; (void)b; (void)bs; return 0;
}
int ata_write_sectors(uint32_t lba, uint32_t c, const void* b, uint32_t bs) {
    (void)lba; (void)c; (void)b; (void)bs; return 0;
}

/* Stub IDT */
static int g_idt_entries[256];
int idt_entry_present(uint8_t index) { return g_idt_entries[index]; }

/* Stub ksys */
static uint64_t g_fake_uptime = 0;
uint64_t ksys_uptime_ms(void) { return g_fake_uptime++; }

/* Include kfs for filesystem check */
#include "../include/kfs.c"

/* Include source under test */
#include "../include/kcheckup.c"

/* Init heap */
static char fake_heap[0x200000] __attribute__((aligned(4096)));

static void setup(void) {
    unsigned int i;

    heap_region_start = (size_t)fake_heap;
    heap_region_end = (size_t)fake_heap + 0x100000;
    heap_start = (block_header_t*)fake_heap;
    heap_start->size = (0x100000 - sizeof(block_header_t)) & ~(HEAP_ALIGNMENT - 1);
    heap_start->free = 1;
    heap_start->magic = BLOCK_MAGIC_FREE;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    g_heap_lock = 0;

    g_ata_available = 0;
    g_lua_valid = 0;
    g_fake_uptime = 0;

    for (i = 0; i < 256; i++) {
        g_idt_entries[i] = 0;
    }

    kfs_init();
}

/* --- Tests --- */

static void test_heap_check_pass(void) {
    TEST_SUITE("checkup heap pass");
    setup();

    ASSERT(kcheckup_heap(), "heap check passes with healthy heap");
}

static void test_idt_check_pass(void) {
    TEST_SUITE("checkup idt pass");
    setup();

    /* Install the critical handlers */
    g_idt_entries[0] = 1;   /* #DE */
    g_idt_entries[8] = 1;   /* #DF */
    g_idt_entries[13] = 1;  /* #GP */
    g_idt_entries[14] = 1;  /* #PF */
    g_idt_entries[32] = 1;  /* IRQ0 */

    ASSERT(kcheckup_idt(), "idt check passes with all handlers");
}

static void test_idt_check_fail_missing(void) {
    TEST_SUITE("checkup idt fail missing");
    setup();

    /* Only install some handlers */
    g_idt_entries[0] = 1;
    g_idt_entries[8] = 1;
    /* 13 missing! */
    g_idt_entries[14] = 1;
    g_idt_entries[32] = 1;

    ASSERT(!kcheckup_idt(), "idt check fails with missing #GP handler");
}

static void test_ata_check_always_pass(void) {
    TEST_SUITE("checkup ata");
    setup();

    /* ATA not available — should still pass */
    g_ata_available = 0;
    ASSERT(kcheckup_ata(), "ata check passes when no disk");

    /* ATA available */
    g_ata_available = 1;
    ASSERT(kcheckup_ata(), "ata check passes when disk present");
}

static void test_lua_check_pass(void) {
    TEST_SUITE("checkup lua pass");
    setup();

    g_lua_valid = 1;
    ASSERT(kcheckup_lua(), "lua check passes with valid VM");
}

static void test_lua_check_fail(void) {
    TEST_SUITE("checkup lua fail");
    setup();

    g_lua_valid = 0;
    ASSERT(!kcheckup_lua(), "lua check fails with NULL state");
}

static void test_fs_check_empty(void) {
    TEST_SUITE("checkup fs empty");
    setup();

    ASSERT(kcheckup_fs(), "fs check passes on empty filesystem");
}

static void test_fs_check_with_files(void) {
    TEST_SUITE("checkup fs with files");
    setup();

    kfs_write("test.txt", "hello");
    kfs_write("data.bin", "world");

    ASSERT(kcheckup_fs(), "fs check passes with valid files");
}

static void test_timer_check(void) {
    TEST_SUITE("checkup timer");
    setup();

    /* g_fake_uptime auto-increments, so timer check should pass */
    ASSERT(kcheckup_timer(), "timer check passes when uptime advances");
}

static void test_run_all_pass(void) {
    TEST_SUITE("checkup run all pass");
    setup();

    /* Set up everything to pass */
    g_idt_entries[0] = 1;
    g_idt_entries[8] = 1;
    g_idt_entries[13] = 1;
    g_idt_entries[14] = 1;
    g_idt_entries[32] = 1;
    g_lua_valid = 1;
    g_ata_available = 1;

    {
        unsigned int fails = kcheckup_run();
        ASSERT_EQ(fails, 0u, "kcheckup_run returns 0 failures");
    }
}

static void test_run_with_failures(void) {
    TEST_SUITE("checkup run with failures");
    setup();

    /* IDT missing handlers, Lua not valid */
    g_idt_entries[0] = 1;
    /* missing 8, 13, 14, 32 */
    g_lua_valid = 0;

    {
        unsigned int fails = kcheckup_run();
        ASSERT(fails >= 2u, "kcheckup_run reports >=2 failures (idt + lua)");
    }
}

/* --- Main --- */

int main(void) {
    test_heap_check_pass();
    test_idt_check_pass();
    test_idt_check_fail_missing();
    test_ata_check_always_pass();
    test_lua_check_pass();
    test_lua_check_fail();
    test_fs_check_empty();
    test_fs_check_with_files();
    test_timer_check();
    test_run_all_pass();
    test_run_with_failures();

    TEST_RESULTS();
}
