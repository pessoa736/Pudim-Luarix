#ifndef FS_COMMON_H
#define FS_COMMON_H

#include "stddef.h"
#include "utypes.h"

#define FS_MAX_NAME       128u
#define FS_MAX_LABEL       64u
#define FS_SECTOR_SIZE    512u

/* File entry returned by list operations */
typedef struct fs_entry {
    char     name[FS_MAX_NAME];
    uint32_t size;
    uint8_t  is_dir;
    uint8_t  is_readonly;
} fs_entry_t;

/* Filesystem info block */
typedef struct fs_info {
    char     label[FS_MAX_LABEL];
    char     type[16];          /* "PLFS", "FAT32", "ext4", "NTFS" */
    uint32_t total_sectors;
    uint32_t free_sectors;
    uint32_t sector_size;
    uint32_t cluster_size;
    uint32_t file_count;
} fs_info_t;

/* Callback for file listing: returns 0 to stop, 1 to continue */
typedef int (*fs_list_cb)(const fs_entry_t* entry, void* ctx);

#endif
