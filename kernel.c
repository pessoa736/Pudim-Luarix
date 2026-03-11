#include "include/vga.h"
#include "include/idt.h"
#include "include/kheap.h"
#include "include/klua.h"
#include "include/kterm.h"
#include "include/serial.h"
#include "include/ksys.h"

static void heap_self_test(void) {
    void* a;
    void* b;
    void* c;
    void* d;

    vga_print("[heap] init ");
    heap_init();

    a = kmalloc(64);
    b = kmalloc(128);
    c = kmalloc(32);

    if (!a || !b || !c) {
        vga_print("FAIL alloc \n");
        return;
    }

    kfree(b);
    d = kmalloc(64);

    if (!d) {
        vga_print("FAIL reuse \n");
        return;
    }

    kfree(a);
    kfree(c);
    kfree(d);

    vga_print("OK \n");
}

void lau_main() {
    // Early marker: write to VGA directly without clearing
    volatile uint16_t* vga = (uint16_t*)0xB8000;
    vga[0] = (0x0F << 8) | 'K';  // White 'K' in the first position
    
    vga_clear();
    vga_print("Pudim-Luarix x86_64 \n");

    set_idt_entry(&ir0, (uint8_t)0x8E, (uint8_t)0);
    load_IDT();
    vga_print("[idt] OK \n");

    heap_self_test();
    ksys_init();

    if (klua_init()) {
        vga_print("[lua] init OK\n");
        klua_run(
            "print('Hello from Lua!'); "
            "kfs.write('boot.log', 'kernel started'); "
            "kfs.append('boot.log', ' | lua ok'); "
            "print('kfs count:', kfs.count()); "
            "print('boot.log:', kfs.read('boot.log')); "
            "print('boot.log size:', kfs.size('boot.log'))"
        );
    } else {
        vga_print("[lua] init FAIL\n");
    }

    vga_print("[main] entering terminal\n");
    kterm_run();

    vga_print("[main] halt\n");

    while (1) {
        asm volatile ("hlt");
    }
}
