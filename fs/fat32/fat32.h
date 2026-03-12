#ifndef FAT32_H
#define FAT32_H

#include "../fs_common.h"

#define FAT32_BOOT_SIG_0     0x55u
#define FAT32_BOOT_SIG_1     0xAAu
#define FAT32_FSTYPE_OFF     82u     /* "FAT32   " at offset 82 in BPB */
#define FAT32_CLUSTER_FREE   0x00000000u
#define FAT32_CLUSTER_EOC    0x0FFFFFF8u  /* end-of-chain marker (>=) */
#define FAT32_CLUSTER_BAD    0x0FFFFFF7u
#define FAT32_ATTR_READ_ONLY 0x01u
#define FAT32_ATTR_HIDDEN    0x02u
#define FAT32_ATTR_SYSTEM    0x04u
#define FAT32_ATTR_VOLUME_ID 0x08u
#define FAT32_ATTR_DIRECTORY 0x10u
#define FAT32_ATTR_ARCHIVE   0x20u
#define FAT32_ATTR_LFN       0x0Fu
#define FAT32_DIR_ENTRY_SIZE 32u
#define FAT32_MAX_READ_SIZE  65536u

/* Parsed BPB (BIOS Parameter Block) */
typedef struct fat32_bpb {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t total_sectors;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    char     volume_label[12];
} fat32_bpb_t;

/* Detect FAT32 at the given LBA offset */
int fat32_detect(uint32_t lba_start);

/* Parse BPB and fill info structure */
int fat32_info(uint32_t lba_start, fs_info_t* out);

/* List files in root directory */
int fat32_list(uint32_t lba_start, fs_list_cb cb, void* ctx);

/* Read a file from root directory by 8.3 name. Returns bytes read. */
uint32_t fat32_read(uint32_t lba_start, const char* name,
                    void* buf, uint32_t bufsize);

#endif
