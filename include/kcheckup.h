#ifndef KCHECKUP_H
#define KCHECKUP_H

/*
 * kcheckup — Boot Checkup System for Pudim-Luarix
 *
 * Pudding analogy: "Taste Test Before Serving"
 *
 * Before serving the pudding (opening the terminal), we run a
 * complete taste test to make sure every ingredient is right,
 * the mold is intact, the oven worked, and the custard set.
 *
 * Each check returns 1 (pass) or 0 (fail).
 * kcheckup_run() runs all checks and prints a pass/fail report.
 */

/* Individual checks — each returns 1 = pass, 0 = fail */
int kcheckup_heap(void);     /* pudding consistency — heap integrity     */
int kcheckup_idt(void);      /* mold integrity     — exception handlers  */
int kcheckup_ata(void);      /* oven status        — disk/ATA check      */
int kcheckup_lua(void);      /* batter ready       — Lua VM valid        */
int kcheckup_fs(void);       /* pantry inventory   — filesystem check    */
int kcheckup_timer(void);    /* oven temperature   — PIT ticks running   */

/* Run all checks and print report via kbootlog.
   Returns number of failures (0 = all passed). */
unsigned int kcheckup_run(void);

#endif
