#include "plfs.h"
#include "ata.h"
#include "serial.h"

/*
 * PLFS on-disk layout (little-endian):
 *
 * Sector 0 (header):
 *   [0..3]   magic "PLFS"
 *   [4..7]   version (uint32 = 1)
 *   [8..11]  file_count (uint32)
 *   [12..511] reserved
 *
 * For each file (sequential after header):
 *   Entry sector:
 *     [0..47]  name (null-terminated, 48 bytes)
 *     [48..51] size (uint32)
 *     [52..511] reserved
 *   Data sectors:
 *     ceil(size / 512) sectors of raw file content
 */

/* ---- Helpers ---- */

static void plfs_put_u32(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFFu);
    buf[1] = (uint8_t)((val >> 8) & 0xFFu);
    buf[2] = (uint8_t)((val >> 16) & 0xFFu);
    buf[3] = (uint8_t)((val >> 24) & 0xFFu);
}

static uint32_t plfs_get_u32(const uint8_t* buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint32_t plfs_strlen(const char* s) {
    uint32_t len = 0;

    if (!s) {
        return 0;
    }

    while (s[len]) {
        len++;
    }

    return len;
}

static int plfs_streq(const char* a, const char* b) {
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

static void plfs_strncpy0(char* dst, const char* src, uint32_t cap) {
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

static void plfs_zero(uint8_t* buf, uint32_t len) {
    uint32_t i;

    for (i = 0; i < len; i++) {
        buf[i] = 0;
    }
}

static int plfs_read_header(uint32_t lba_start, uint8_t* sector,
                             uint32_t* file_count) {
    if (!ata_is_available()) {
        return 0;
    }

    if (!ata_read_sectors(lba_start, 1, sector, FS_SECTOR_SIZE)) {
        return 0;
    }

    if (sector[0] != PLFS_MAGIC_0 || sector[1] != PLFS_MAGIC_1 ||
        sector[2] != PLFS_MAGIC_2 || sector[3] != PLFS_MAGIC_3) {
        return 0;
    }

    if (plfs_get_u32(&sector[4]) != PLFS_VERSION) {
        return 0;
    }

    *file_count = plfs_get_u32(&sector[8]);

    if (*file_count > PLFS_MAX_FILES) {
        return 0;
    }

    return 1;
}

/* ---- Public API ---- */

int plfs_detect(uint32_t lba_start) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t count;

    return plfs_read_header(lba_start, sector, &count);
}

int plfs_info(uint32_t lba_start, fs_info_t* out) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t count;

    if (!out) {
        return 0;
    }

    if (!plfs_read_header(lba_start, sector, &count)) {
        return 0;
    }

    plfs_strncpy0(out->label, "PLFS Volume", sizeof(out->label));
    plfs_strncpy0(out->type, "PLFS", sizeof(out->type));
    out->total_sectors = PLFS_MAX_SECTORS;
    out->free_sectors = 0;
    out->sector_size = FS_SECTOR_SIZE;
    out->cluster_size = FS_SECTOR_SIZE;
    out->file_count = count;

    return 1;
}

int plfs_list(uint32_t lba_start, fs_list_cb cb, void* ctx) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t count;
    uint32_t lba;
    uint32_t f;

    if (!cb) {
        return 0;
    }

    if (!plfs_read_header(lba_start, sector, &count)) {
        return 0;
    }

    lba = lba_start + 1;

    for (f = 0; f < count; f++) {
        fs_entry_t entry;
        uint32_t fsize;
        uint32_t data_sectors;

        if (!ata_read_sectors(lba, 1, sector, FS_SECTOR_SIZE)) {
            return 0;
        }

        plfs_strncpy0(entry.name, (const char*)sector, sizeof(entry.name));
        fsize = plfs_get_u32(&sector[48]);

        if (fsize > PLFS_MAX_FILE_SIZE) {
            return 0;
        }

        entry.size = fsize;
        entry.is_dir = 0;
        entry.is_readonly = 0;

        data_sectors = (fsize + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
        lba += 1 + data_sectors;

        if (!cb(&entry, ctx)) {
            break;
        }
    }

    return 1;
}

uint32_t plfs_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize) {
    uint8_t sector[FS_SECTOR_SIZE];
    uint32_t count;
    uint32_t lba;
    uint32_t f;

    if (!name || !buf || bufsize == 0) {
        return 0;
    }

    if (!plfs_read_header(lba_start, sector, &count)) {
        return 0;
    }

    lba = lba_start + 1;

    for (f = 0; f < count; f++) {
        char fname[PLFS_MAX_NAME];
        uint32_t fsize;
        uint32_t data_sectors;

        if (!ata_read_sectors(lba, 1, sector, FS_SECTOR_SIZE)) {
            return 0;
        }

        plfs_strncpy0(fname, (const char*)sector, PLFS_MAX_NAME);
        fsize = plfs_get_u32(&sector[48]);

        if (fsize > PLFS_MAX_FILE_SIZE) {
            return 0;
        }

        data_sectors = (fsize + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
        lba++;

        if (plfs_streq(fname, name)) {
            uint32_t copied = 0;
            uint32_t s;
            uint8_t* dst = (uint8_t*)buf;

            for (s = 0; s < data_sectors && copied < fsize; s++) {
                uint32_t remain = fsize - copied;
                uint32_t chunk = remain < FS_SECTOR_SIZE ? remain : FS_SECTOR_SIZE;
                uint32_t i;

                if (copied + chunk > bufsize) {
                    chunk = bufsize - copied;
                }

                if (!ata_read_sectors(lba + s, 1, sector, FS_SECTOR_SIZE)) {
                    return 0;
                }

                for (i = 0; i < chunk; i++) {
                    dst[copied + i] = sector[i];
                }

                copied += chunk;
            }

            return copied;
        }

        lba += data_sectors;
    }

    return 0;
}

int plfs_write(uint32_t lba_start, const char* name,
               const void* data, uint32_t size) {
    /* Full rewrite: read all entries, replace/add, write back */
    /* For simplicity, this delegates to the in-memory kfs layer.
     * Direct disk write is already handled by kfs_persist_save().
     * This function provides the standalone PLFS interface. */
    (void)lba_start;
    (void)name;
    (void)data;
    (void)size;

    serial_print("[plfs] write: use kfs API for write operations\r\n");
    return 0;
}

int plfs_delete(uint32_t lba_start, const char* name) {
    (void)lba_start;
    (void)name;

    serial_print("[plfs] delete: use kfs API for delete operations\r\n");
    return 0;
}

int plfs_format(uint32_t lba_start) {
    uint8_t sector[FS_SECTOR_SIZE];

    if (!ata_is_available()) {
        return 0;
    }

    plfs_zero(sector, FS_SECTOR_SIZE);

    sector[0] = PLFS_MAGIC_0;
    sector[1] = PLFS_MAGIC_1;
    sector[2] = PLFS_MAGIC_2;
    sector[3] = PLFS_MAGIC_3;
    plfs_put_u32(&sector[4], PLFS_VERSION);
    plfs_put_u32(&sector[8], 0);

    if (!ata_write_sectors(lba_start, 1, sector, FS_SECTOR_SIZE)) {
        serial_print("[plfs] format: write fail\r\n");
        return 0;
    }

    serial_print("[plfs] format: OK\r\n");
    return 1;
}
