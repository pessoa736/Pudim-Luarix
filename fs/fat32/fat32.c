#include "fat32.h"
#include "ata.h"
#include "serial.h"

/*
 * FAT32 on-disk layout:
 *
 * Sector 0: Boot sector / BPB
 *   [0..2]   jmpBoot
 *   [3..10]  OEMName
 *   [11..12] BytsPerSec (uint16)
 *   [13]     SecPerClus
 *   [14..15] RsvdSecCnt (uint16)
 *   [16]     NumFATs
 *   [17..18] RootEntCnt (must be 0 for FAT32)
 *   [19..20] TotSec16 (0 for FAT32)
 *   [21]     Media
 *   [22..23] FATSz16 (0 for FAT32)
 *   [32..35] TotSec32 (uint32)
 *   [36..39] FATSz32 (uint32)
 *   [44..47] RootClus (uint32)
 *   [71..81] BS_VolLab (11 bytes)
 *   [82..89] BS_FilSysType "FAT32   "
 *   [510..511] 0x55 0xAA
 *
 * After reserved sectors: FAT table(s)
 * After FATs: data region (cluster 2 = first data cluster)
 * Root directory: chain starting at RootClus
 *
 * Directory entry (32 bytes):
 *   [0..10]  DIR_Name (8.3 format, space-padded)
 *   [11]     DIR_Attr
 *   [20..21] DIR_FstClusHI
 *   [26..27] DIR_FstClusLO
 *   [28..31] DIR_FileSize
 */

/* ---- Helpers ---- */

static uint16_t fat32_get_u16(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t fat32_get_u32(const uint8_t* buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static void fat32_strncpy0(char* dst, const char* src, uint32_t cap) {
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

static int fat32_parse_bpb(uint32_t lba_start, fat32_bpb_t* bpb) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t i;

    if (!ata_is_available()) {
        return 0;
    }

    if (!ata_read_sectors(lba_start, 1, sector, FS_SECTOR_SIZE)) {
        return 0;
    }

    /* Check boot signature */
    if (sector[510] != FAT32_BOOT_SIG_0 || sector[511] != FAT32_BOOT_SIG_1) {
        return 0;
    }

    bpb->bytes_per_sector = fat32_get_u16(&sector[11]);
    bpb->sectors_per_cluster = sector[13];
    bpb->reserved_sectors = fat32_get_u16(&sector[14]);
    bpb->num_fats = sector[16];
    bpb->total_sectors = fat32_get_u32(&sector[32]);
    bpb->fat_size_sectors = fat32_get_u32(&sector[36]);
    bpb->root_cluster = fat32_get_u32(&sector[44]);

    /* Volume label (11 bytes at offset 71) */
    for (i = 0; i < 11 && i + 1 < sizeof(bpb->volume_label); i++) {
        bpb->volume_label[i] = (char)sector[71 + i];
    }
    bpb->volume_label[i] = 0;

    /* Trim trailing spaces */
    while (i > 0 && bpb->volume_label[i - 1] == ' ') {
        bpb->volume_label[--i] = 0;
    }

    /* Validate: FAT32 must have 0 root entry count and 0 FATSz16 */
    if (fat32_get_u16(&sector[17]) != 0) {
        return 0;
    }

    if (fat32_get_u16(&sector[22]) != 0) {
        return 0;
    }

    /* Basic sanity */
    if (bpb->bytes_per_sector == 0 || bpb->sectors_per_cluster == 0 ||
        bpb->num_fats == 0 || bpb->fat_size_sectors == 0) {
        return 0;
    }

    return 1;
}

/* Convert cluster number to LBA */
static uint32_t fat32_cluster_to_lba(const fat32_bpb_t* bpb,
                                      uint32_t lba_start, uint32_t cluster) {
    uint32_t data_start = lba_start + bpb->reserved_sectors
                        + (bpb->num_fats * bpb->fat_size_sectors);

    return data_start + (cluster - 2) * bpb->sectors_per_cluster;
}

/* Read the next cluster from the FAT */
static uint32_t fat32_next_cluster(const fat32_bpb_t* bpb,
                                    uint32_t lba_start, uint32_t cluster) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t fat_offset;
    uint32_t fat_sector;
    uint32_t entry_offset;
    uint32_t fat_lba;
    uint32_t value;

    fat_offset = cluster * 4;
    fat_sector = fat_offset / FS_SECTOR_SIZE;
    entry_offset = fat_offset % FS_SECTOR_SIZE;

    fat_lba = lba_start + bpb->reserved_sectors + fat_sector;

    if (!ata_read_sectors(fat_lba, 1, sector, FS_SECTOR_SIZE)) {
        return FAT32_CLUSTER_EOC;
    }

    value = fat32_get_u32(&sector[entry_offset]) & 0x0FFFFFFFu;

    return value;
}

/* Format an 8.3 name from directory entry into a readable string */
static void fat32_format_83name(const uint8_t* raw, char* out, uint32_t outsize) {
    uint32_t i;
    uint32_t pos = 0;

    if (!out || outsize < 2) {
        return;
    }

    /* Name part (8 bytes) */
    for (i = 0; i < 8 && pos + 1 < outsize; i++) {
        if (raw[i] == ' ') {
            break;
        }

        out[pos++] = (char)raw[i];
    }

    /* Extension part (3 bytes) */
    if (raw[8] != ' ') {
        if (pos + 1 < outsize) {
            out[pos++] = '.';
        }

        for (i = 8; i < 11 && pos + 1 < outsize; i++) {
            if (raw[i] == ' ') {
                break;
            }

            out[pos++] = (char)raw[i];
        }
    }

    out[pos] = 0;
}

/* Compare two strings case-insensitively */
static int fat32_strcasecmp(const char* a, const char* b) {
    uint32_t i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'a' && ca <= 'z') {
            ca -= 32;
        }

        if (cb >= 'a' && cb <= 'z') {
            cb -= 32;
        }

        if (ca != cb) {
            return 0;
        }

        i++;
    }

    return a[i] == b[i];
}

/* ---- Public API ---- */

int fat32_detect(uint32_t lba_start) {
    fat32_bpb_t bpb;

    return fat32_parse_bpb(lba_start, &bpb);
}

int fat32_info(uint32_t lba_start, fs_info_t* out) {
    fat32_bpb_t bpb;

    if (!out) {
        return 0;
    }

    if (!fat32_parse_bpb(lba_start, &bpb)) {
        return 0;
    }

    fat32_strncpy0(out->label, bpb.volume_label, sizeof(out->label));
    fat32_strncpy0(out->type, "FAT32", sizeof(out->type));
    out->total_sectors = bpb.total_sectors;
    out->free_sectors = 0; /* would need full FAT scan */
    out->sector_size = bpb.bytes_per_sector;
    out->cluster_size = (uint32_t)bpb.sectors_per_cluster * bpb.bytes_per_sector;
    out->file_count = 0; /* filled by list if needed */

    return 1;
}

int fat32_list(uint32_t lba_start, fs_list_cb cb, void* ctx) {
    fat32_bpb_t bpb;
    uint32_t cluster;
    uint32_t chain_limit = 1024;

    if (!cb) {
        return 0;
    }

    if (!fat32_parse_bpb(lba_start, &bpb)) {
        return 0;
    }

    cluster = bpb.root_cluster;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_BAD && chain_limit > 0) {
        uint32_t lba = fat32_cluster_to_lba(&bpb, lba_start, cluster);
        uint32_t s;

        for (s = 0; s < bpb.sectors_per_cluster; s++) {
            uint8_t sector[FS_SECTOR_SIZE];
            uint32_t entries_per_sector = FS_SECTOR_SIZE / FAT32_DIR_ENTRY_SIZE;
            uint32_t e;

            if (!ata_read_sectors(lba + s, 1, sector, FS_SECTOR_SIZE)) {
                return 0;
            }

            for (e = 0; e < entries_per_sector; e++) {
                uint8_t* ent = &sector[e * FAT32_DIR_ENTRY_SIZE];
                uint8_t first_byte = ent[0];
                uint8_t attr = ent[11];
                fs_entry_t entry;
                uint32_t fsize;

                /* End of directory */
                if (first_byte == 0x00) {
                    return 1;
                }

                /* Deleted entry */
                if (first_byte == 0xE5) {
                    continue;
                }

                /* LFN entry */
                if (attr == FAT32_ATTR_LFN) {
                    continue;
                }

                /* Volume ID */
                if (attr & FAT32_ATTR_VOLUME_ID) {
                    continue;
                }

                fat32_format_83name(ent, entry.name, sizeof(entry.name));
                fsize = fat32_get_u32(&ent[28]);
                entry.size = fsize;
                entry.is_dir = (attr & FAT32_ATTR_DIRECTORY) ? 1 : 0;
                entry.is_readonly = (attr & FAT32_ATTR_READ_ONLY) ? 1 : 0;

                if (!cb(&entry, ctx)) {
                    return 1;
                }
            }
        }

        cluster = fat32_next_cluster(&bpb, lba_start, cluster);
        chain_limit--;
    }

    return 1;
}

uint32_t fat32_read(uint32_t lba_start, const char* name,
                    void* buf, uint32_t bufsize) {
    fat32_bpb_t bpb;
    uint32_t cluster;
    uint32_t chain_limit = 1024;

    if (!name || !buf || bufsize == 0) {
        return 0;
    }

    if (!fat32_parse_bpb(lba_start, &bpb)) {
        return 0;
    }

    /* Search root directory for the file */
    cluster = bpb.root_cluster;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_BAD && chain_limit > 0) {
        uint32_t lba = fat32_cluster_to_lba(&bpb, lba_start, cluster);
        uint32_t s;

        for (s = 0; s < bpb.sectors_per_cluster; s++) {
            uint8_t sector[FS_SECTOR_SIZE];
            uint32_t entries_per_sector = FS_SECTOR_SIZE / FAT32_DIR_ENTRY_SIZE;
            uint32_t e;

            if (!ata_read_sectors(lba + s, 1, sector, FS_SECTOR_SIZE)) {
                return 0;
            }

            for (e = 0; e < entries_per_sector; e++) {
                uint8_t* ent = &sector[e * FAT32_DIR_ENTRY_SIZE];
                uint8_t first_byte = ent[0];
                uint8_t attr = ent[11];
                char entry_name[16];
                uint32_t fsize;
                uint32_t file_cluster;
                uint32_t copied = 0;
                uint32_t file_chain_limit = 2048;

                if (first_byte == 0x00) {
                    return 0;
                }

                if (first_byte == 0xE5) {
                    continue;
                }

                if (attr == FAT32_ATTR_LFN || (attr & FAT32_ATTR_VOLUME_ID)) {
                    continue;
                }

                if (attr & FAT32_ATTR_DIRECTORY) {
                    continue;
                }

                fat32_format_83name(ent, entry_name, sizeof(entry_name));

                if (!fat32_strcasecmp(entry_name, name)) {
                    continue;
                }

                /* Found the file — read its cluster chain */
                fsize = fat32_get_u32(&ent[28]);
                file_cluster = ((uint32_t)fat32_get_u16(&ent[20]) << 16)
                             | (uint32_t)fat32_get_u16(&ent[26]);

                if (fsize > bufsize) {
                    fsize = bufsize;
                }

                if (fsize > FAT32_MAX_READ_SIZE) {
                    fsize = FAT32_MAX_READ_SIZE;
                }

                while (file_cluster >= 2 && file_cluster < FAT32_CLUSTER_BAD &&
                       copied < fsize && file_chain_limit > 0) {
                    uint32_t clba = fat32_cluster_to_lba(&bpb, lba_start, file_cluster);
                    uint32_t cs;

                    for (cs = 0; cs < bpb.sectors_per_cluster && copied < fsize; cs++) {
                        uint8_t sec[FS_SECTOR_SIZE];
                        uint32_t remain = fsize - copied;
                        uint32_t chunk = remain < FS_SECTOR_SIZE ? remain : FS_SECTOR_SIZE;
                        uint32_t i;

                        if (!ata_read_sectors(clba + cs, 1, sec, FS_SECTOR_SIZE)) {
                            return copied;
                        }

                        for (i = 0; i < chunk; i++) {
                            ((uint8_t*)buf)[copied + i] = sec[i];
                        }

                        copied += chunk;
                    }

                    file_cluster = fat32_next_cluster(&bpb, lba_start, file_cluster);
                    file_chain_limit--;
                }

                return copied;
            }
        }

        cluster = fat32_next_cluster(&bpb, lba_start, cluster);
        chain_limit--;
    }

    return 0;
}
