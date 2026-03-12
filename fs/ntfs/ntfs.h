#ifndef NTFS_H
#define NTFS_H

#include "../fs_common.h"

#define NTFS_MAGIC_0 'N'
#define NTFS_MAGIC_1 'T'
#define NTFS_MAGIC_2 'F'
#define NTFS_MAGIC_3 'S'
#define NTFS_OEM_OFFSET      3u     /* "NTFS    " at offset 3 */
#define NTFS_BOOT_SIG_0      0x55u
#define NTFS_BOOT_SIG_1      0xAAu
#define NTFS_FILE_SIG_0      'F'
#define NTFS_FILE_SIG_1      'I'
#define NTFS_FILE_SIG_2      'L'
#define NTFS_FILE_SIG_3      'E'
#define NTFS_MFT_ENTRY_ROOT  5u
#define NTFS_MAX_READ_SIZE   65536u

/* Attribute types */
#define NTFS_ATTR_STANDARD_INFO  0x10u
#define NTFS_ATTR_FILE_NAME      0x30u
#define NTFS_ATTR_DATA           0x80u
#define NTFS_ATTR_INDEX_ROOT     0x90u
#define NTFS_ATTR_INDEX_ALLOC    0xA0u
#define NTFS_ATTR_END            0xFFFFFFFFu

/* File name namespace */
#define NTFS_NS_POSIX 0u
#define NTFS_NS_WIN32 1u
#define NTFS_NS_DOS   2u
#define NTFS_NS_WIN32_DOS 3u

/* Parsed boot sector fields */
typedef struct ntfs_boot {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint64_t total_sectors;
    uint64_t mft_cluster;       /* MFT start cluster number */
    uint32_t mft_record_size;   /* bytes per MFT record */
} ntfs_boot_t;

/* Detect NTFS at the given LBA offset */
int ntfs_detect(uint32_t lba_start);

/* Get filesystem info */
int ntfs_info(uint32_t lba_start, fs_info_t* out);

/* List files in root directory */
int ntfs_list(uint32_t lba_start, fs_list_cb cb, void* ctx);

/* Read a file from root directory by name. Returns bytes read. */
uint32_t ntfs_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize);

#endif
