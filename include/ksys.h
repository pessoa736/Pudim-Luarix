#ifndef KSYS_H
#define KSYS_H

#include <stdint.h>

/* System information and timing */

/* Get kernel version string */
const char* ksys_version(void);

/* Get system uptime in milliseconds since boot */
uint64_t ksys_uptime_ms(void);

/* Get system uptime in microseconds since boot */
uint64_t ksys_uptime_us(void);

/* Get total RAM in bytes */
uint64_t ksys_memory_total(void);

/* Get free RAM in bytes */
uint64_t ksys_memory_free(void);

/* Get used RAM in bytes */
uint64_t ksys_memory_used(void);

/* Get number of CPUs */
uint32_t ksys_cpu_count(void);

/* Initialize system info (call from kernel init) */
void ksys_init(void);

/* Update uptime counter (call from timer ISR) */
void ksys_tick(void);

#endif
