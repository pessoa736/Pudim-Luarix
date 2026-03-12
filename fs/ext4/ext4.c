#include "ext4.h"
#include "ata.h"
#include "serial.h"

/*
 * ext4 on-disk layout:
 *
 * Bytes 0-1023: Boot block (unused by ext4)
 * Bytes 1024-2047: Superblock
 *   [0..3]   s_inodes_count (uint32)
 *   [4..7]   s_blocks_count_lo (uint32)
 *   [24..27] s_log_block_size (uint32) => block_size = 1024 << this
 *   [32..35] s_blocks_per_group (uint32)
 *   [40..43] s_inodes_per_group (uint32)
 *   [56..57] s_magic (uint16) == 0xEF53
 *   [88..91] s_inode_size (uint16 at 88)
 *   [120..135] s_volume_name (16 bytes)
 *
 * Block group descriptor table: starts at block (first_data_block + 1)
 *   Each entry is 32 or 64 bytes:
 *     [8..11]  bg_inode_table_lo (uint32)
 *
 * Inode table: block pointed to by bg_inode_table
 *   Each inode is inode_size bytes:
 *     [0..1]   i_mode (uint16)
 *     [4..7]   i_size_lo (uint32)
 *     [28..31] i_flags (uint32)
 *     [40..99] i_block[15] or extent tree (60 bytes)
 *
 * Directory entries (linear):
 *   [0..3]   inode (uint32)
 *   [4..5]   rec_len (uint16)
 *   [6]      name_len (uint8)
 *   [7]      file_type (uint8)
 *   [8..]    name (name_len bytes)
 */

/* ---- Helpers ---- */

static uint16_t ext4_get_u16(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t ext4_get_u32(const uint8_t* buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static void ext4_strncpy0(char* dst, const char* src, uint32_t cap) {
    uint32_t i = 0;

    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static int ext4_streq_n(const char* a, const char* b, uint32_t blen) {
    uint32_t i = 0;

    if (!a || !b) {
        return 0;
    }

    while (i < blen && a[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return i == blen && a[i] == 0;
}

/* Read bytes at an arbitrary byte offset from disk into buf */
static int ext4_read_bytes(uint32_t lba_start, uint32_t byte_offset,
                            uint8_t* buf, uint32_t count) {
    uint32_t sector = byte_offset / FS_SECTOR_SIZE;
    uint32_t offset = byte_offset % FS_SECTOR_SIZE;
    uint8_t tmp[FS_SECTOR_SIZE];
    uint32_t copied = 0;

    while (copied < count) {
        uint32_t remain = count - copied;
        uint32_t avail = FS_SECTOR_SIZE - offset;
        uint32_t chunk = remain < avail ? remain : avail;
        uint32_t i;

        if (!ata_read_sectors(lba_start + sector, 1, tmp, FS_SECTOR_SIZE)) {
            return 0;
        }

        for (i = 0; i < chunk; i++) {
            buf[copied + i] = tmp[offset + i];
        }

        copied += chunk;
        sector++;
        offset = 0;
    }

    return 1;
}

static int ext4_parse_super(uint32_t lba_start, ext4_super_t* sb) {
    uint8_t raw[264];
    uint32_t log_block_size;
    uint32_t i;

    if (!ata_is_available()) {
        return 0;
    }

    /* Read superblock at offset 1024 */
    if (!ext4_read_bytes(lba_start, EXT4_SUPER_OFFSET, raw, 264)) {
        return 0;
    }

    sb->magic = ext4_get_u16(&raw[56]);

    if (sb->magic != EXT4_MAGIC) {
        return 0;
    }

    sb->inodes_count = ext4_get_u32(&raw[0]);
    sb->blocks_count = ext4_get_u32(&raw[4]);

    log_block_size = ext4_get_u32(&raw[24]);

    if (log_block_size > 6) {
        return 0; /* unreasonable: > 64KB blocks */
    }

    sb->block_size = 1024u << log_block_size;
    sb->blocks_per_group = ext4_get_u32(&raw[32]);
    sb->inodes_per_group = ext4_get_u32(&raw[40]);
    sb->inode_size = ext4_get_u16(&raw[88]);
    sb->first_data_block = ext4_get_u32(&raw[20]);

    /* Volume name at offset 120, 16 bytes */
    for (i = 0; i < 16; i++) {
        sb->volume_name[i] = (char)raw[120 + i];
    }
    sb->volume_name[16] = 0;

    /* Trim trailing nulls/spaces */
    i = 16;
    while (i > 0 && (sb->volume_name[i - 1] == 0 || sb->volume_name[i - 1] == ' ')) {
        sb->volume_name[--i] = 0;
    }

    if (sb->inode_size < EXT4_GOOD_OLD_INODE_SZ) {
        sb->inode_size = EXT4_GOOD_OLD_INODE_SZ;
    }

    return 1;
}

/* Get the LBA of a block number */
static uint32_t ext4_block_to_byte(const ext4_super_t* sb, uint32_t block) {
    return block * sb->block_size;
}

/* Read the inode table block for a given inode number */
static int ext4_read_inode(uint32_t lba_start, const ext4_super_t* sb,
                            uint32_t inode_num, uint8_t* inode_buf) {
    uint32_t group;
    uint32_t index_in_group;
    uint32_t bg_desc_byte;
    uint8_t bg_raw[64];
    uint32_t inode_table_block;
    uint32_t inode_byte_offset;

    if (inode_num == 0) {
        return 0;
    }

    group = (inode_num - 1) / sb->inodes_per_group;
    index_in_group = (inode_num - 1) % sb->inodes_per_group;

    /* Block group descriptor table starts at block (first_data_block + 1) */
    bg_desc_byte = ext4_block_to_byte(sb, sb->first_data_block + 1)
                 + group * 64;

    if (!ext4_read_bytes(lba_start, bg_desc_byte, bg_raw, 64)) {
        return 0;
    }

    inode_table_block = ext4_get_u32(&bg_raw[8]);
    inode_byte_offset = ext4_block_to_byte(sb, inode_table_block)
                      + index_in_group * sb->inode_size;

    if (!ext4_read_bytes(lba_start, inode_byte_offset, inode_buf, sb->inode_size)) {
        return 0;
    }

    return 1;
}

/*
 * Read data from an inode using direct block pointers (i_block[0..11]).
 * This covers files up to 12 * block_size (typically 48KB with 4K blocks).
 * Extent trees (EXT4_INODE_EXTENTS_FL) are also supported at the root level.
 */
static uint32_t ext4_read_inode_data(uint32_t lba_start, const ext4_super_t* sb,
                                      const uint8_t* inode_buf,
                                      uint8_t* out, uint32_t outsize) {
    uint32_t file_size = ext4_get_u32(&inode_buf[4]);
    uint32_t flags = ext4_get_u32(&inode_buf[28]);
    uint32_t to_read;
    uint32_t copied = 0;

    if (file_size > outsize) {
        file_size = outsize;
    }

    if (file_size > EXT4_MAX_READ_SIZE) {
        file_size = EXT4_MAX_READ_SIZE;
    }

    to_read = file_size;

    if (flags & EXT4_INODE_EXTENTS_FL) {
        /* Extent tree: i_block starts with extent header at offset 40 */
        const uint8_t* eh = &inode_buf[40];
        uint16_t eh_magic = ext4_get_u16(&eh[0]);
        uint16_t eh_entries = ext4_get_u16(&eh[2]);
        uint16_t eh_depth = ext4_get_u16(&eh[4]);
        uint32_t e;

        if (eh_magic != 0xF30A || eh_depth != 0) {
            /* Only handle leaf extents (depth 0) */
            return 0;
        }

        for (e = 0; e < eh_entries && copied < to_read; e++) {
            const uint8_t* ext = &eh[12 + e * 12];
            uint16_t ee_len = ext4_get_u16(&ext[4]);
            uint32_t ee_start_lo = ext4_get_u32(&ext[8]);
            uint32_t b;

            for (b = 0; b < ee_len && copied < to_read; b++) {
                uint32_t block_byte = ext4_block_to_byte(sb, ee_start_lo + b);
                uint32_t remain = to_read - copied;
                uint32_t chunk = remain < sb->block_size ? remain : sb->block_size;

                if (!ext4_read_bytes(lba_start, block_byte, &out[copied], chunk)) {
                    return copied;
                }

                copied += chunk;
            }
        }
    } else {
        /* Direct block pointers: i_block[0..11] at offset 40 */
        uint32_t b;

        for (b = 0; b < 12 && copied < to_read; b++) {
            uint32_t block_num = ext4_get_u32(&inode_buf[40 + b * 4]);
            uint32_t block_byte;
            uint32_t remain;
            uint32_t chunk;

            if (block_num == 0) {
                break;
            }

            block_byte = ext4_block_to_byte(sb, block_num);
            remain = to_read - copied;
            chunk = remain < sb->block_size ? remain : sb->block_size;

            if (!ext4_read_bytes(lba_start, block_byte, &out[copied], chunk)) {
                return copied;
            }

            copied += chunk;
        }
    }

    return copied;
}

/* ---- Public API ---- */

int ext4_detect(uint32_t lba_start) {
    ext4_super_t sb;

    return ext4_parse_super(lba_start, &sb);
}

int ext4_info(uint32_t lba_start, fs_info_t* out) {
    ext4_super_t sb;

    if (!out) {
        return 0;
    }

    if (!ext4_parse_super(lba_start, &sb)) {
        return 0;
    }

    ext4_strncpy0(out->label, sb.volume_name, sizeof(out->label));
    ext4_strncpy0(out->type, "ext4", sizeof(out->type));
    out->total_sectors = sb.blocks_count * (sb.block_size / FS_SECTOR_SIZE);
    out->free_sectors = 0;
    out->sector_size = FS_SECTOR_SIZE;
    out->cluster_size = sb.block_size;
    out->file_count = sb.inodes_count;

    return 1;
}

int ext4_list(uint32_t lba_start, fs_list_cb cb, void* ctx) {
    ext4_super_t sb;
    uint8_t inode_buf[256];
    uint32_t dir_size;
    uint32_t pos;
    uint8_t dir_data[4096];
    uint32_t read_len;

    if (!cb) {
        return 0;
    }

    if (!ext4_parse_super(lba_start, &sb)) {
        return 0;
    }

    /* Read root inode (inode 2) */
    if (!ext4_read_inode(lba_start, &sb, EXT4_ROOT_INODE, inode_buf)) {
        return 0;
    }

    dir_size = ext4_get_u32(&inode_buf[4]);

    if (dir_size > sizeof(dir_data)) {
        dir_size = sizeof(dir_data);
    }

    read_len = ext4_read_inode_data(lba_start, &sb, inode_buf,
                                     dir_data, sizeof(dir_data));

    if (read_len == 0) {
        return 0;
    }

    /* Parse linear directory entries */
    pos = 0;

    while (pos + 8 < read_len) {
        uint32_t d_inode = ext4_get_u32(&dir_data[pos]);
        uint16_t rec_len = ext4_get_u16(&dir_data[pos + 4]);
        uint8_t name_len = dir_data[pos + 6];
        uint8_t file_type = dir_data[pos + 7];

        if (rec_len == 0) {
            break;
        }

        if (d_inode != 0 && name_len > 0 && pos + 8 + name_len <= read_len) {
            fs_entry_t entry;
            uint32_t copy_len;

            copy_len = name_len;
            if (copy_len >= FS_MAX_NAME) {
                copy_len = FS_MAX_NAME - 1;
            }

            {
                uint32_t i;
                for (i = 0; i < copy_len; i++) {
                    entry.name[i] = (char)dir_data[pos + 8 + i];
                }
                entry.name[copy_len] = 0;
            }

            entry.is_dir = (file_type == EXT4_FT_DIR) ? 1 : 0;
            entry.is_readonly = 0;
            entry.size = 0;

            /* Try to get file size from inode */
            if (!entry.is_dir && d_inode <= sb.inodes_count) {
                uint8_t child_inode[256];
                if (ext4_read_inode(lba_start, &sb, d_inode, child_inode)) {
                    entry.size = ext4_get_u32(&child_inode[4]);
                }
            }

            if (!cb(&entry, ctx)) {
                return 1;
            }
        }

        pos += rec_len;
    }

    return 1;
}

uint32_t ext4_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize) {
    ext4_super_t sb;
    uint8_t inode_buf[256];
    uint32_t dir_size;
    uint32_t pos;
    uint8_t dir_data[4096];
    uint32_t read_len;

    if (!name || !buf || bufsize == 0) {
        return 0;
    }

    if (!ext4_parse_super(lba_start, &sb)) {
        return 0;
    }

    /* Read root inode */
    if (!ext4_read_inode(lba_start, &sb, EXT4_ROOT_INODE, inode_buf)) {
        return 0;
    }

    dir_size = ext4_get_u32(&inode_buf[4]);

    if (dir_size > sizeof(dir_data)) {
        dir_size = sizeof(dir_data);
    }

    read_len = ext4_read_inode_data(lba_start, &sb, inode_buf,
                                     dir_data, sizeof(dir_data));

    if (read_len == 0) {
        return 0;
    }

    /* Find file in directory */
    pos = 0;

    while (pos + 8 < read_len) {
        uint32_t d_inode = ext4_get_u32(&dir_data[pos]);
        uint16_t rec_len = ext4_get_u16(&dir_data[pos + 4]);
        uint8_t name_len = dir_data[pos + 6];
        uint8_t file_type = dir_data[pos + 7];

        if (rec_len == 0) {
            break;
        }

        if (d_inode != 0 && name_len > 0 &&
            file_type == EXT4_FT_REG_FILE &&
            pos + 8 + name_len <= read_len) {

            if (ext4_streq_n(name, (const char*)&dir_data[pos + 8], name_len)) {
                /* Found — read its inode data */
                uint8_t file_inode[256];

                if (!ext4_read_inode(lba_start, &sb, d_inode, file_inode)) {
                    return 0;
                }

                return ext4_read_inode_data(lba_start, &sb, file_inode,
                                             (uint8_t*)buf, bufsize);
            }
        }

        pos += rec_len;
    }

    return 0;
}
