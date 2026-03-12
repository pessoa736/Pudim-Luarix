#include "kcheckup.h"
#include "kbootlog.h"
#include "kheap.h"
#include "idt.h"
#include "ata.h"
#include "kfs.h"
#include "ksys.h"
#include "arch.h"
#include "APISLua/klua.h"

/*
 * kcheckup — "Taste Test Before Serving"
 *
 * A full checkup of all kernel subsystems before handing
 * control to the terminal. Like a chef doing a final taste
 * test before plating the pudding.
 */

/* ---- Pudding Consistency: heap integrity ---- */

int kcheckup_heap(void) {
    size_t total;
    size_t free_b;
    unsigned int blocks;
    void* probe;

    total = kheap_total_bytes();
    free_b = kheap_free_bytes();
    blocks = kheap_block_count();

    /* Heap must have some capacity */
    if (total == 0 || blocks == 0) {
        return 0;
    }

    /* Free must not exceed total */
    if (free_b > total) {
        return 0;
    }

    /* Probe: allocate and free a small block */
    probe = kmalloc(16);
    if (!probe) {
        return 0;
    }
    kfree(probe);

    return 1;
}

/* ---- Mold Integrity: IDT exception handlers ---- */

int kcheckup_idt(void) {
    /* Check critical exception handlers are installed:
       0  = #DE (divide error)
       8  = #DF (double fault)
       13 = #GP (general protection)
       14 = #PF (page fault)
       32 = IRQ0 (timer) */

    if (!idt_entry_present(0)) {
        return 0;
    }

    if (!idt_entry_present(8)) {
        return 0;
    }

    if (!idt_entry_present(13)) {
        return 0;
    }

    if (!idt_entry_present(14)) {
        return 0;
    }

    if (!idt_entry_present(32)) {
        return 0;
    }

    return 1;
}

/* ---- Oven Status: ATA/disk availability ---- */

int kcheckup_ata(void) {
    /* ATA is optional — pass if not available (no oven is not a failure,
       just means we can't bake persistent data) */
    /* We just verify the driver doesn't crash when queried */
    (void)ata_is_available();
    return 1;
}

/* ---- Batter Ready: Lua VM state ---- */

int kcheckup_lua(void) {
    lua_State* L = klua_state();

    if (!L) {
        return 0;
    }

    /* Verify core tables are registered by checking a known global */
    lua_getglobal(L, "print");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    lua_pop(L, 1);

    /* Check that kfs table exists */
    lua_getglobal(L, "kfs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    lua_pop(L, 1);

    /* Check that sys table exists */
    lua_getglobal(L, "sys");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    lua_pop(L, 1);

    return 1;
}

/* ---- Pantry Inventory: filesystem consistency ---- */

int kcheckup_fs(void) {
    size_t count = kfs_count();
    size_t i;

    /* Empty filesystem is fine — just check for consistency */
    for (i = 0; i < count; i++) {
        const char* name = kfs_name_at(i);

        /* Every slot must have a valid name */
        if (!name || !name[0]) {
            return 0;
        }

        /* Name must exist in kfs_exists */
        if (!kfs_exists(name)) {
            return 0;
        }
    }

    return 1;
}

/* ---- Oven Temperature: timer/PIT ticking ---- */

int kcheckup_timer(void) {
    uint64_t t1;
    uint64_t t2;
    volatile unsigned int spin;

    t1 = ksys_uptime_ms();

    /* Spin briefly to let at least one tick happen (PIT at 100Hz = 10ms) */
    for (spin = 0; spin < 2000000u; spin++) {
        arch_nop();
    }

    t2 = ksys_uptime_ms();

    /* Uptime should have advanced (timer interrupts are firing) */
    return t2 > t1;
}

/* ---- Taste Test Report ---- */

unsigned int kcheckup_run(void) {
    unsigned int fails = 0;

    kbootlog_title("checkup");
    kbootlog_writeln("taste test before serving...");

    /* Pudding consistency */
    if (kcheckup_heap()) {
        kbootlog_line("checkup", "heap (pudding consistency)  PASS");
    } else {
        kbootlog_line("checkup", "heap (pudding consistency)  FAIL");
        fails++;
    }

    /* Mold integrity */
    if (kcheckup_idt()) {
        kbootlog_line("checkup", "idt  (mold integrity)      PASS");
    } else {
        kbootlog_line("checkup", "idt  (mold integrity)      FAIL");
        fails++;
    }

    /* Oven status */
    if (kcheckup_ata()) {
        if (ata_is_available()) {
            kbootlog_line("checkup", "ata  (oven status)         PASS (disk present)");
        } else {
            kbootlog_line("checkup", "ata  (oven status)         PASS (no disk)");
        }
    } else {
        kbootlog_line("checkup", "ata  (oven status)         FAIL");
        fails++;
    }

    /* Batter ready */
    if (kcheckup_lua()) {
        kbootlog_line("checkup", "lua  (batter ready)        PASS");
    } else {
        kbootlog_line("checkup", "lua  (batter ready)        FAIL");
        fails++;
    }

    /* Pantry inventory */
    if (kcheckup_fs()) {
        kbootlog_line("checkup", "fs   (pantry inventory)    PASS");
    } else {
        kbootlog_line("checkup", "fs   (pantry inventory)    FAIL");
        fails++;
    }

    /* Oven temperature */
    if (kcheckup_timer()) {
        kbootlog_line("checkup", "timer(oven temperature)    PASS");
    } else {
        kbootlog_line("checkup", "timer(oven temperature)    FAIL");
        fails++;
    }

    /* Summary */
    if (fails == 0) {
        kbootlog_line("checkup", "all 6 checks PASSED - ready to serve!");
    } else {
        kbootlog_title("checkup");
        kbootlog_writeln("WARNING: some checks FAILED");
    }

    return fails;
}
