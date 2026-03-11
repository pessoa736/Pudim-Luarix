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

/* Get free heap bytes */
uint64_t ksys_memory_free(void);

/* Get used heap bytes */
uint64_t ksys_memory_used(void);

/* Get total heap bytes */
uint64_t ksys_heap_total(void);

/* Get boot media size in bytes */
uint64_t ksys_boot_media_total(void);

/* Get boot media/ROM size in bytes */
uint64_t ksys_rom_total(void);

/* Get kernel image size in bytes */
uint64_t ksys_kernel_image_size(void);

/* Get number of logical CPUs */
uint32_t ksys_cpu_count(void);

/* Get number of physical CPU cores */
uint32_t ksys_cpu_physical_cores(void);

/* Get CPU vendor string */
const char* ksys_cpu_vendor(void);

/* Get CPU brand string */
const char* ksys_cpu_brand(void);

/* Get count of captured firmware memory map entries */
uint32_t ksys_memory_map_count(void);

/* Get firmware memory map entry base address */
uint64_t ksys_memory_map_base(uint32_t index);

/* Get firmware memory map entry length */
uint64_t ksys_memory_map_length(uint32_t index);

/* Get firmware memory map entry type */
uint32_t ksys_memory_map_type(uint32_t index);

/* Get firmware memory map entry base address as hex */
const char* ksys_memory_map_base_hex(uint32_t index);

/* Get firmware memory map entry length as hex */
const char* ksys_memory_map_length_hex(uint32_t index);

/* Initialize system info (call from kernel init) */
void ksys_init(void);

/* Update uptime counter (call from timer ISR) */
void ksys_tick(void);

#endif
