/*
 * ata_aarch64.c — ATA stubs for aarch64
 * No IDE/ATA on QEMU virt machine.
 */
#include "ata.h"
#include "utypes.h"

int ata_init(void) { return 0; }
int ata_is_available(void) { return 0; }

int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buf_size) {
    (void)lba; (void)count; (void)buffer; (void)buf_size;
    return 0;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buf_size) {
    (void)lba; (void)count; (void)buffer; (void)buf_size;
    return 0;
}
