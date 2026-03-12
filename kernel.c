#include "include/arch.h"
#include "include/kdisplay.h"
#include "include/serial.h"

#if ARCH_HAS_VGA
#include "include/vga.h"
#endif

#include "include/idt.h"
#include "include/ata.h"
#include "include/kbootlog.h"
#include "include/kheap.h"
#include "include/kio.h"
#include "include/APISLua/klua.h"
#include "include/kfs.h"
#include "include/kprocess.h"
#include "include/kterm.h"
#include "include/ktimer.h"
#include "include/kevent.h"
#include "include/ksys.h"
#include "include/mouse.h"
#include "include/ksecurity.h"
#include "include/kdebug.h"
#include "include/kcheckup.h"

static void u32_to_str(unsigned int value, char* out, unsigned int out_size) {
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

static void selftest_log_pids(unsigned int p1, unsigned int p2) {
    char p1s[16];
    char p2s[16];

    u32_to_str(p1, p1s, sizeof(p1s));
    u32_to_str(p2, p2s, sizeof(p2s));

    kbootlog_title("selftest");
    kbootlog_write("pids ");
    kbootlog_write(p1s);
    kbootlog_write(" ");
    kbootlog_writeln(p2s);
}

static void boot_delay_tick(void) {
    volatile unsigned int i;

    for (i = 0; i < 8000000u; i++) {
        arch_nop();
    }
}

static void boot_countdown_to_terminal(void) {
    kbootlog_line("boot", "clearing screen and opening terminal");
    kbootlog_line("boot", "countdown 3");
    boot_delay_tick();
    kbootlog_line("boot", "countdown 2");
    boot_delay_tick();
    kbootlog_line("boot", "countdown 1");
    boot_delay_tick();
    kio_clear();
}

static void heap_self_test(void) {
    void* a;
    void* b;
    void* c;
    void* d;

    kbootlog_title("heap");
    kbootlog_write("selftest ");

    a = kmalloc(64);
    b = kmalloc(128);
    c = kmalloc(32);

    if (!a || !b || !c) {
        kbootlog_writeln("FAIL alloc");
        return;
    }

    kfree(b);
    d = kmalloc(64);

    if (!d) {
        kbootlog_writeln("FAIL reuse");
        return;
    }

    kfree(a);
    kfree(c);
    kfree(d);

    kbootlog_writeln("OK");
}

void lau_main() {
    serial_init();
    kdisplay_init();

#if ARCH_HAS_VGA
    if (kdisplay_mode() == KDISPLAY_MODE_VGA) {
        /* Early marker: write to VGA directly without clearing */
        volatile uint16_t* vga = (uint16_t*)0xB8000;
        vga[0] = (0x0F << 8) | 'K';  /* White 'K' in the first position */

        vga_clear();
    }
#endif

    kdisplay_write("Pudim-Luarix " ARCH_NAME " \n");

#if ARCH_X86_64
    set_idt_entry(&ir0, (uint8_t)0x8E, (uint8_t)0);
    set_idt_entry(&ir8, (uint8_t)0x8E, (uint8_t)8);
    set_idt_entry(&ir13, (uint8_t)0x8E, (uint8_t)13);
    set_idt_entry(&ir14, (uint8_t)0x8E, (uint8_t)14);
    set_idt_entry(&irq0, (uint8_t)0x8E, (uint8_t)32);
    set_idt_entry(&irq4, (uint8_t)0x8E, (uint8_t)36);
    set_idt_entry(&irq12, (uint8_t)0x8E, (uint8_t)44);
    load_IDT();
    ktimer_init(100);
    serial_init_irq();
    mouse_init();
#endif

    arch_enable_interrupts();

    heap_init();
    kbootlog_enable_file();

    kbootlog_line("idt", "OK");

    heap_self_test();
    ksys_init();
    ksecurity_init();
    kbootlog_line("security", "init OK");
    kdebug_history_init();
    kdebug_timer_reset();
    kbootlog_line("debug", "pudding layers ready");
    kevent_init();
    kbootlog_line("event", "init OK");

    if (ata_init()) {
        kbootlog_line("ata", "storage disk OK");
        if (kfs_persist_load()) {
            kbootlog_line("kfs", "loaded from disk");
        } else {
            kbootlog_line("kfs", "no saved data (fresh disk)");
        }
    } else {
        kbootlog_line("ata", "no storage disk");
    }

    if (klua_init()) {
        kbootlog_line("lua", "init OK");
        klua_run(
            "local _print = print; "
            "local print = function(...)"
            "   _print(...);"
            "   local args = {...}; "
            "   local l = ''; " 
            "   for idx, v in ipairs(args) do "
            "       l = l .. tostring(v) .. ' '; "  
            "   end "
            "   l = l .. '\\n'; "
            "   kfs.append('boot.log', l); "
            "end;"
            ""
            "print('Hello from Lua!'); "
            "print('kernel started'); "
            "print('kfs count:', kfs.count()); "
            "print('boot.log size:', kfs.size('boot.log')); "
            "print('boot.log', ' | lua ok'); "
        );

        /* Run init.lua from filesystem if present */
        if (kfs_exists("init.lua")) {
            const char* init_code = kfs_read("init.lua");
            if (init_code) {
                kbootlog_line("init", "running init.lua");
                if (klua_run_quiet(init_code)) {
                    kbootlog_line("init", "OK");
                } else {
                    kbootlog_line("init", "FAIL");
                }
            }
        }

        {
            unsigned int p1;
            unsigned int p2;

            kbootlog_line("selftest", "process scheduler start");

            p1 = (unsigned int)kprocess_create("local x = 1; for i=1,3000 do x = x + 1; end");
            p2 = (unsigned int)kprocess_create("local x = 100; for i=1,3000 do x = x + 10; end");

            selftest_log_pids(p1, p2);
            kbootlog_line("selftest", "process scheduler armed");
        }
    } else {
        kbootlog_line("lua", "init FAIL");
    }

    {
        int i;
        kbootlog_line("boot", "running scheduler ticks");
        for (i = 0; i < 8; i++) {
            int r = kprocess_tick();
            if (r) {
                kbootlog_line("boot", "tick done r=1");
            } else {
                kbootlog_line("boot", "tick done r=0");
            }
        }
        kbootlog_line("boot", "ticks complete");
    }

    kcheckup_run();

    kbootlog_line("main", "entering terminal");
    boot_countdown_to_terminal();
    kbootlog_writeln("\nwelcome to Pudim-luarix " ARCH_NAME "\n");
    kterm_run();

    kbootlog_line("main", "halt");

    while (1) {
        arch_halt();
    }
}
