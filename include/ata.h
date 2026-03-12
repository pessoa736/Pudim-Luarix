#ifndef ATA_H
#define ATA_H

#include "stddef.h"
#include <stdint.h>

#define ATA_SECTOR_SIZE 512u

int ata_init(void);
int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buffer_size);
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buffer_size);
int ata_is_available(void);

#endif
