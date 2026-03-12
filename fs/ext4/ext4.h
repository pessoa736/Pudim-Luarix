#ifndef EXT4_H
#define EXT4_H

#include "../fs_common.h"

#define EXT4_MAGIC              0xEF53u
#define EXT4_SUPER_OFFSET       1024u   /* superblock starts at byte 1024 */
#define EXT4_ROOT_INODE         2u
#define EXT4_GOOD_OLD_INODE_SZ  128u
#define EXT4_MAX_READ_SIZE      65536u

/* Inode flags */
#define EXT4_INODE_EXTENTS_FL   0x00080000u

/* File type in directory entry (stored in file_type field) */
#define EXT4_FT_UNKNOWN  0u
#define EXT4_FT_REG_FILE 1u
#define EXT4_FT_DIR      2u
#define EXT4_FT_CHRDEV   3u
#define EXT4_FT_BLKDEV   4u
#define EXT4_FT_FIFO     5u
#define EXT4_FT_SOCK     6u
#define EXT4_FT_SYMLINK  7u

/* Parsed superblock fields */
typedef struct ext4_super {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t block_size;       /* bytes */
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint16_t inode_size;
    uint32_t first_data_block;
    char     volume_name[17];
    uint16_t magic;
} ext4_super_t;

/* Detect ext4 at the given LBA offset */
int ext4_detect(uint32_t lba_start);

/* Get filesystem info */
int ext4_info(uint32_t lba_start, fs_info_t* out);

/* List files in root directory */
int ext4_list(uint32_t lba_start, fs_list_cb cb, void* ctx);

/* Read a file from root directory by name. Returns bytes read. */
uint32_t ext4_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize);

#endif
