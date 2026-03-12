#include "ntfs.h"
#include "ata.h"
#include "serial.h"

/*
 * NTFS on-disk layout:
 *
 * Sector 0: Boot sector
 *   [0..2]   jmpBoot
 *   [3..10]  OEM "NTFS    "
 *   [11..12] BytsPerSec (uint16)
 *   [13]     SecPerClus
 *   [40..47] TotalSectors (uint64)
 *   [48..55] MFT cluster (uint64)
 *   [64]     MFT record size: if positive, clusters per record;
 *            if negative, 2^|val| bytes
 *   [510..511] 0x55 0xAA
 *
 * MFT Record (typically 1024 bytes):
 *   [0..3]   Signature "FILE"
 *   [4..5]   Update sequence offset
 *   [6..7]   Update sequence size
 *   [20..21] First attribute offset (uint16)
 *   [22..23] Flags
 *   [24..27] Used size (uint32)
 *   [28..31] Allocated size (uint32)
 *
 * Attribute header:
 *   [0..3]   Type (uint32)
 *   [4..7]   Length (uint32)
 *   [8]      Non-resident flag
 *   [9]      Name length (characters)
 *   [10..11] Name offset
 *
 * Resident attribute:
 *   [16..19] Value length (uint32)
 *   [20..21] Value offset (uint16)
 *
 * $FILE_NAME attribute (0x30), resident value:
 *   [64]     Name length (characters, UTF-16)
 *   [65]     Namespace
 *   [66..]   Name (UTF-16LE, name_len * 2 bytes)
 *
 * $INDEX_ROOT (0x90) contains the B-tree of directory entries.
 */

/* ---- Helpers ---- */

static uint16_t ntfs_get_u16(const uint8_t* buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t ntfs_get_u32(const uint8_t* buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint64_t ntfs_get_u64(const uint8_t* buf) {
    return (uint64_t)ntfs_get_u32(buf)
         | ((uint64_t)ntfs_get_u32(buf + 4) << 32);
}

static void ntfs_strncpy0(char* dst, const char* src, uint32_t cap) {
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

/* Read bytes at arbitrary offset from partition start */
static int ntfs_read_bytes(uint32_t lba_start, uint64_t byte_offset,
                            uint8_t* buf, uint32_t count) {
    uint32_t sector = (uint32_t)(byte_offset / FS_SECTOR_SIZE);
    uint32_t offset = (uint32_t)(byte_offset % FS_SECTOR_SIZE);
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

static int ntfs_parse_boot(uint32_t lba_start, ntfs_boot_t* boot) {
    uint8_t sector[FS_SECTOR_SIZE];
    int8_t rec_size_raw;

    if (!ata_is_available()) {
        return 0;
    }

    if (!ata_read_sectors(lba_start, 1, sector, FS_SECTOR_SIZE)) {
        return 0;
    }

    /* Check boot signature */
    if (sector[510] != NTFS_BOOT_SIG_0 || sector[511] != NTFS_BOOT_SIG_1) {
        return 0;
    }

    /* Check OEM "NTFS    " */
    if (sector[3] != 'N' || sector[4] != 'T' ||
        sector[5] != 'F' || sector[6] != 'S') {
        return 0;
    }

    boot->bytes_per_sector = ntfs_get_u16(&sector[11]);
    boot->sectors_per_cluster = sector[13];
    boot->total_sectors = ntfs_get_u64(&sector[40]);
    boot->mft_cluster = ntfs_get_u64(&sector[48]);

    /* MFT record size: offset 64 */
    rec_size_raw = (int8_t)sector[64];

    if (rec_size_raw > 0) {
        boot->mft_record_size = (uint32_t)rec_size_raw
                              * boot->sectors_per_cluster
                              * boot->bytes_per_sector;
    } else {
        boot->mft_record_size = 1u << (uint32_t)(-rec_size_raw);
    }

    /* Sanity checks */
    if (boot->bytes_per_sector == 0 || boot->sectors_per_cluster == 0 ||
        boot->mft_record_size == 0 || boot->mft_record_size > 4096) {
        return 0;
    }

    return 1;
}

/* Get byte offset of an MFT record */
static uint64_t ntfs_mft_record_offset(const ntfs_boot_t* boot, uint32_t record_num) {
    uint64_t cluster_size = (uint64_t)boot->sectors_per_cluster
                          * boot->bytes_per_sector;
    uint64_t mft_byte = boot->mft_cluster * cluster_size;

    return mft_byte + (uint64_t)record_num * boot->mft_record_size;
}

/* Read an MFT record into buffer. Buffer must be >= mft_record_size. */
static int ntfs_read_mft_record(uint32_t lba_start, const ntfs_boot_t* boot,
                                 uint32_t record_num, uint8_t* buf) {
    uint64_t offset = ntfs_mft_record_offset(boot, record_num);

    if (!ntfs_read_bytes(lba_start, offset, buf, boot->mft_record_size)) {
        return 0;
    }

    /* Check FILE signature */
    if (buf[0] != NTFS_FILE_SIG_0 || buf[1] != NTFS_FILE_SIG_1 ||
        buf[2] != NTFS_FILE_SIG_2 || buf[3] != NTFS_FILE_SIG_3) {
        return 0;
    }

    /* Apply fixup (update sequence) */
    {
        uint16_t usa_offset = ntfs_get_u16(&buf[4]);
        uint16_t usa_count = ntfs_get_u16(&buf[6]);
        uint16_t fixup_val;
        uint32_t i;

        if (usa_offset >= boot->mft_record_size || usa_count == 0) {
            return 1; /* skip fixup if invalid */
        }

        fixup_val = ntfs_get_u16(&buf[usa_offset]);

        for (i = 1; i < usa_count; i++) {
            uint32_t pos = i * FS_SECTOR_SIZE - 2;

            if (pos + 1 >= boot->mft_record_size) {
                break;
            }

            /* Validate fixup */
            if (ntfs_get_u16(&buf[pos]) != fixup_val) {
                return 0;
            }

            /* Apply correction */
            buf[pos] = buf[usa_offset + i * 2];
            buf[pos + 1] = buf[usa_offset + i * 2 + 1];
        }
    }

    return 1;
}

/* Find an attribute in an MFT record. Returns pointer within buf or NULL. */
static const uint8_t* ntfs_find_attr(const uint8_t* mft_buf,
                                      uint32_t mft_size, uint32_t attr_type) {
    uint16_t first_attr = ntfs_get_u16(&mft_buf[20]);
    uint32_t off = first_attr;

    while (off + 4 < mft_size) {
        uint32_t atype = ntfs_get_u32(&mft_buf[off]);
        uint32_t alen;

        if (atype == NTFS_ATTR_END || atype == 0) {
            break;
        }

        alen = ntfs_get_u32(&mft_buf[off + 4]);

        if (alen == 0 || off + alen > mft_size) {
            break;
        }

        if (atype == attr_type) {
            return &mft_buf[off];
        }

        off += alen;
    }

    return (void*)0;
}

/* Extract file name from $FILE_NAME attribute (UTF-16LE to ASCII) */
static int ntfs_get_filename(const uint8_t* fn_attr, uint32_t fn_attr_len,
                              char* out, uint32_t outsize, uint8_t* namespace_out) {
    uint8_t non_resident;
    uint32_t val_len;
    uint16_t val_offset;
    const uint8_t* val;
    uint8_t name_len;
    uint32_t i;
    uint32_t pos = 0;

    if (fn_attr_len < 24) {
        return 0;
    }

    non_resident = fn_attr[8];

    if (non_resident) {
        return 0; /* $FILE_NAME should always be resident */
    }

    val_len = ntfs_get_u32(&fn_attr[16]);
    val_offset = ntfs_get_u16(&fn_attr[20]);

    if (val_offset + val_len > fn_attr_len || val_len < 66) {
        return 0;
    }

    val = &fn_attr[val_offset];
    name_len = val[64];
    *namespace_out = val[65];

    if (66u + name_len * 2u > val_len) {
        return 0;
    }

    for (i = 0; i < name_len && pos + 1 < outsize; i++) {
        uint16_t wchar = ntfs_get_u16(&val[66 + i * 2]);

        /* Simple UTF-16 to ASCII */
        if (wchar > 0 && wchar < 128) {
            out[pos++] = (char)wchar;
        } else {
            out[pos++] = '?';
        }
    }

    out[pos] = 0;
    return pos > 0;
}

/* Compare two ASCII strings */
static int ntfs_streq(const char* a, const char* b) {
    uint32_t i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }

    return a[i] == b[i];
}

/* Read resident $DATA attribute content */
static uint32_t ntfs_read_resident_data(const uint8_t* data_attr,
                                         uint32_t data_attr_len,
                                         uint8_t* out, uint32_t outsize) {
    uint32_t val_len;
    uint16_t val_offset;
    uint32_t copy_len;
    uint32_t i;

    if (data_attr[8] != 0) {
        return 0; /* non-resident — not handled here */
    }

    val_len = ntfs_get_u32(&data_attr[16]);
    val_offset = ntfs_get_u16(&data_attr[20]);

    if (val_offset + val_len > data_attr_len) {
        return 0;
    }

    copy_len = val_len;

    if (copy_len > outsize) {
        copy_len = outsize;
    }

    if (copy_len > NTFS_MAX_READ_SIZE) {
        copy_len = NTFS_MAX_READ_SIZE;
    }

    for (i = 0; i < copy_len; i++) {
        out[i] = data_attr[val_offset + i];
    }

    return copy_len;
}

/* Parse data runs for non-resident $DATA. Read file content from clusters. */
static uint32_t ntfs_read_nonresident_data(uint32_t lba_start,
                                            const ntfs_boot_t* boot,
                                            const uint8_t* data_attr,
                                            uint32_t data_attr_len,
                                            uint8_t* out, uint32_t outsize) {
    uint64_t real_size;
    uint16_t runs_offset;
    uint32_t rpos;
    uint32_t copied = 0;
    int64_t prev_lcn = 0;
    uint32_t cluster_bytes;

    if (data_attr[8] != 1) {
        return 0;
    }

    real_size = ntfs_get_u64(&data_attr[48]);
    runs_offset = ntfs_get_u16(&data_attr[32]);

    if (real_size > outsize) {
        real_size = outsize;
    }

    if (real_size > NTFS_MAX_READ_SIZE) {
        real_size = NTFS_MAX_READ_SIZE;
    }

    cluster_bytes = (uint32_t)boot->sectors_per_cluster * boot->bytes_per_sector;
    rpos = runs_offset;

    while (rpos < data_attr_len && copied < (uint32_t)real_size) {
        uint8_t header = data_attr[rpos];
        uint8_t len_size;
        uint8_t off_size;
        uint64_t run_length = 0;
        int64_t run_offset = 0;
        int64_t lcn;
        uint32_t c;

        if (header == 0) {
            break;
        }

        len_size = header & 0x0F;
        off_size = (header >> 4) & 0x0F;
        rpos++;

        if (len_size > 4 || off_size > 4) {
            break;
        }

        if (rpos + len_size + off_size > data_attr_len) {
            break;
        }

        /* Read run length */
        {
            uint32_t i;
            for (i = 0; i < len_size; i++) {
                run_length |= (uint64_t)data_attr[rpos + i] << (i * 8);
            }
            rpos += len_size;
        }

        /* Read run offset (signed) */
        {
            uint32_t i;
            for (i = 0; i < off_size; i++) {
                run_offset |= (int64_t)data_attr[rpos + i] << (i * 8);
            }
            /* Sign extend */
            if (off_size > 0 && (data_attr[rpos + off_size - 1] & 0x80)) {
                uint32_t i2;
                for (i2 = off_size; i2 < 8; i2++) {
                    run_offset |= (int64_t)0xFF << (i2 * 8);
                }
            }
            rpos += off_size;
        }

        if (off_size == 0) {
            /* Sparse run — skip */
            continue;
        }

        lcn = prev_lcn + run_offset;
        prev_lcn = lcn;

        for (c = 0; c < (uint32_t)run_length && copied < (uint32_t)real_size; c++) {
            uint64_t cluster_byte = (uint64_t)lcn * cluster_bytes
                                  + (uint64_t)c * cluster_bytes;
            uint32_t remain = (uint32_t)real_size - copied;
            uint32_t chunk = remain < cluster_bytes ? remain : cluster_bytes;

            if (!ntfs_read_bytes(lba_start, cluster_byte, &out[copied], chunk)) {
                return copied;
            }

            copied += chunk;
        }
    }

    return copied;
}

/* ---- Public API ---- */

int ntfs_detect(uint32_t lba_start) {
    ntfs_boot_t boot;

    return ntfs_parse_boot(lba_start, &boot);
}

int ntfs_info(uint32_t lba_start, fs_info_t* out) {
    ntfs_boot_t boot;

    if (!out) {
        return 0;
    }

    if (!ntfs_parse_boot(lba_start, &boot)) {
        return 0;
    }

    ntfs_strncpy0(out->label, "NTFS Volume", sizeof(out->label));
    ntfs_strncpy0(out->type, "NTFS", sizeof(out->type));
    out->total_sectors = (uint32_t)(boot.total_sectors & 0xFFFFFFFFu);
    out->free_sectors = 0;
    out->sector_size = boot.bytes_per_sector;
    out->cluster_size = (uint32_t)boot.sectors_per_cluster * boot.bytes_per_sector;
    out->file_count = 0;

    return 1;
}

int ntfs_list(uint32_t lba_start, fs_list_cb cb, void* ctx) {
    ntfs_boot_t boot;
    uint8_t mft_buf[4096]; /* max MFT record = 4096 */
    const uint8_t* idx_attr;
    uint32_t idx_len;
    uint16_t idx_val_off;
    uint32_t idx_val_len;
    const uint8_t* idx_root;
    uint32_t node_off;
    uint32_t entries_off;
    uint32_t entries_size;

    if (!cb) {
        return 0;
    }

    if (!ntfs_parse_boot(lba_start, &boot)) {
        return 0;
    }

    if (boot.mft_record_size > sizeof(mft_buf)) {
        return 0;
    }

    /* Read root directory MFT record (record 5) */
    if (!ntfs_read_mft_record(lba_start, &boot, NTFS_MFT_ENTRY_ROOT, mft_buf)) {
        return 0;
    }

    /* Find $INDEX_ROOT attribute */
    idx_attr = ntfs_find_attr(mft_buf, boot.mft_record_size, NTFS_ATTR_INDEX_ROOT);

    if (!idx_attr) {
        return 0;
    }

    idx_len = ntfs_get_u32(&idx_attr[4]);

    if (idx_attr[8] != 0) {
        return 0; /* $INDEX_ROOT must be resident */
    }

    idx_val_off = ntfs_get_u16(&idx_attr[20]);
    idx_val_len = ntfs_get_u32(&idx_attr[16]);

    if (idx_val_off + idx_val_len > idx_len) {
        return 0;
    }

    idx_root = &idx_attr[idx_val_off];

    /*
     * Index root value:
     *   [0..3]   Attribute type (0x30 for $FILE_NAME)
     *   [4..7]   Collation rule
     *   [8..11]  Bytes per index record
     *   [12]     Clusters per index record
     *   [16..19] Offset to first index entry (relative to [16])
     *   [20..23] Total size of index entries + header
     *   [24..27] Allocated size
     *   [28]     Flags: 0x01 = large index (INDEX_ALLOCATION used)
     */
    if (idx_val_len < 32) {
        return 0;
    }

    entries_off = ntfs_get_u32(&idx_root[16]) + 16;
    entries_size = ntfs_get_u32(&idx_root[20]) + 16;

    if (entries_off >= idx_val_len) {
        return 0;
    }

    if (entries_size > idx_val_len) {
        entries_size = idx_val_len;
    }

    node_off = entries_off;

    /*
     * Index entry:
     *   [0..7]   MFT reference (uint64, low 48 bits = record number)
     *   [8..9]   Length of this entry (uint16)
     *   [10..11] Length of stream (file name) (uint16)
     *   [12..15] Flags: 0x01 = has sub-node, 0x02 = last entry
     *   [16..]   Stream (FILE_NAME structure)
     */
    while (node_off + 16 < entries_size) {
        uint16_t entry_len = ntfs_get_u16(&idx_root[node_off + 8]);
        uint16_t stream_len = ntfs_get_u16(&idx_root[node_off + 10]);
        uint32_t entry_flags = ntfs_get_u32(&idx_root[node_off + 12]);
        const uint8_t* stream;
        uint8_t name_len;
        uint8_t ns;
        fs_entry_t entry;
        uint32_t i;
        uint32_t pos;

        if (entry_len == 0) {
            break;
        }

        if (entry_flags & 0x02) {
            break; /* last entry marker */
        }

        if (stream_len < 68 || node_off + 16 + stream_len > entries_size) {
            node_off += entry_len;
            continue;
        }

        stream = &idx_root[node_off + 16];
        name_len = stream[64];
        ns = stream[65];

        /* Skip DOS-only names */
        if (ns == NTFS_NS_DOS) {
            node_off += entry_len;
            continue;
        }

        /* Extract name (UTF-16LE to ASCII) */
        pos = 0;

        for (i = 0; i < name_len && pos + 1 < FS_MAX_NAME; i++) {
            uint16_t wchar = 0;

            if (66 + i * 2 + 1 < stream_len) {
                wchar = ntfs_get_u16(&stream[66 + i * 2]);
            }

            if (wchar > 0 && wchar < 128) {
                entry.name[pos++] = (char)wchar;
            } else {
                entry.name[pos++] = '?';
            }
        }

        entry.name[pos] = 0;

        /* File size from FILE_NAME (real size at offset 48, uint64) */
        entry.size = (uint32_t)ntfs_get_u64(&stream[48]);
        entry.is_dir = (ntfs_get_u32(&stream[56]) & 0x10000000u) ? 1 : 0;
        entry.is_readonly = (ntfs_get_u32(&stream[56]) & 0x00000001u) ? 1 : 0;

        if (!cb(&entry, ctx)) {
            return 1;
        }

        node_off += entry_len;
    }

    return 1;
}

uint32_t ntfs_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize) {
    ntfs_boot_t boot;
    uint8_t mft_buf[4096];
    const uint8_t* idx_attr;
    uint32_t idx_len;
    uint16_t idx_val_off;
    uint32_t idx_val_len;
    const uint8_t* idx_root;
    uint32_t entries_off;
    uint32_t entries_size;
    uint32_t node_off;

    if (!name || !buf || bufsize == 0) {
        return 0;
    }

    if (!ntfs_parse_boot(lba_start, &boot)) {
        return 0;
    }

    if (boot.mft_record_size > sizeof(mft_buf)) {
        return 0;
    }

    /* Read root directory */
    if (!ntfs_read_mft_record(lba_start, &boot, NTFS_MFT_ENTRY_ROOT, mft_buf)) {
        return 0;
    }

    idx_attr = ntfs_find_attr(mft_buf, boot.mft_record_size, NTFS_ATTR_INDEX_ROOT);

    if (!idx_attr || idx_attr[8] != 0) {
        return 0;
    }

    idx_len = ntfs_get_u32(&idx_attr[4]);
    idx_val_off = ntfs_get_u16(&idx_attr[20]);
    idx_val_len = ntfs_get_u32(&idx_attr[16]);

    if (idx_val_off + idx_val_len > idx_len || idx_val_len < 32) {
        return 0;
    }

    idx_root = &idx_attr[idx_val_off];
    entries_off = ntfs_get_u32(&idx_root[16]) + 16;
    entries_size = ntfs_get_u32(&idx_root[20]) + 16;

    if (entries_off >= idx_val_len) {
        return 0;
    }

    if (entries_size > idx_val_len) {
        entries_size = idx_val_len;
    }

    node_off = entries_off;

    while (node_off + 16 < entries_size) {
        uint64_t mft_ref = ntfs_get_u64(&idx_root[node_off]);
        uint32_t mft_record_num = (uint32_t)(mft_ref & 0x0000FFFFFFFFFFFFull);
        uint16_t entry_len = ntfs_get_u16(&idx_root[node_off + 8]);
        uint16_t stream_len = ntfs_get_u16(&idx_root[node_off + 10]);
        uint32_t entry_flags = ntfs_get_u32(&idx_root[node_off + 12]);
        char entry_name[FS_MAX_NAME];
        uint8_t name_len;
        uint8_t ns;
        uint32_t i;
        uint32_t pos;

        if (entry_len == 0) {
            break;
        }

        if (entry_flags & 0x02) {
            break;
        }

        if (stream_len < 68 || node_off + 16 + stream_len > entries_size) {
            node_off += entry_len;
            continue;
        }

        {
            const uint8_t* stream = &idx_root[node_off + 16];
            name_len = stream[64];
            ns = stream[65];

            if (ns == NTFS_NS_DOS) {
                node_off += entry_len;
                continue;
            }

            pos = 0;

            for (i = 0; i < name_len && pos + 1 < sizeof(entry_name); i++) {
                uint16_t wchar = 0;

                if (66 + i * 2 + 1 < stream_len) {
                    wchar = ntfs_get_u16(&stream[66 + i * 2]);
                }

                if (wchar > 0 && wchar < 128) {
                    entry_name[pos++] = (char)wchar;
                } else {
                    entry_name[pos++] = '?';
                }
            }

            entry_name[pos] = 0;
        }

        if (ntfs_streq(entry_name, name)) {
            /* Found — read the file's MFT record and its $DATA */
            const uint8_t* data_attr;
            uint32_t data_len;

            if (!ntfs_read_mft_record(lba_start, &boot, mft_record_num, mft_buf)) {
                return 0;
            }

            data_attr = ntfs_find_attr(mft_buf, boot.mft_record_size, NTFS_ATTR_DATA);

            if (!data_attr) {
                return 0;
            }

            data_len = ntfs_get_u32(&data_attr[4]);

            if (data_attr[8] == 0) {
                return ntfs_read_resident_data(data_attr, data_len,
                                                (uint8_t*)buf, bufsize);
            } else {
                return ntfs_read_nonresident_data(lba_start, &boot,
                                                   data_attr, data_len,
                                                   (uint8_t*)buf, bufsize);
            }
        }

        node_off += entry_len;
    }

    return 0;
}
