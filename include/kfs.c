#include "kfs.h"
#include "kheap.h"
#include "ata.h"
#include "serial.h"

#define KFS_MAX_FILES 32
#define KFS_MAX_NAME  48
#define KFS_MAX_FILE_SIZE 65536
#define KFS_MAX_TOTAL_SIZE (KFS_MAX_FILE_SIZE * KFS_MAX_FILES)

/* Disk format constants */
#define KFS_DISK_MAGIC_0 'P'
#define KFS_DISK_MAGIC_1 'L'
#define KFS_DISK_MAGIC_2 'F'
#define KFS_DISK_MAGIC_3 'S'
#define KFS_DISK_VERSION 1u
#define KFS_SECTOR_SIZE  512u
/* Max sectors: 1 header + 32*(1 entry + 128 data) = 4129 */
#define KFS_DISK_MAX_SECTORS 4200u

typedef struct kfs_file {
    int used;
    char name[KFS_MAX_NAME];
    char* data;
    size_t size;
} kfs_file_t;

static kfs_file_t g_kfs_files[KFS_MAX_FILES];
static volatile uint32_t g_kfs_lock = 0;

static inline void kfs_lock(void) {
    while (1) {
        uint32_t expected = 0;
        uint32_t desired = 1;
        uint32_t oldval;

        asm volatile(
            "lock cmpxchgl %2, %1"
            : "=a"(oldval), "+m"(g_kfs_lock)
            : "r"(desired), "0"(expected)
            : "cc"
        );

        if (oldval == expected) {
            return;
        }

        asm volatile("pause");
    }
}

static inline void kfs_unlock(void) {
    g_kfs_lock = 0;
}

static size_t kfs_strlen(const char* s) {
    size_t len = 0;

    if (!s) {
        return 0;
    }

    while (s[len]) {
        len++;
    }

    return len;
}

static int kfs_streq(const char* a, const char* b) {
    size_t i = 0;

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

static void kfs_strncpy0(char* dst, const char* src, size_t cap) {
    size_t i = 0;

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

static int kfs_is_valid_name(const char* name) {
    size_t len;

    if (!name) {
        return 0;
    }

    len = kfs_strlen(name);
    return len > 0 && len < KFS_MAX_NAME;
}

static int kfs_find_index(const char* name) {
    int i;

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (g_kfs_files[i].used && kfs_streq(g_kfs_files[i].name, name)) {
            return i;
        }
    }

    return -1;
}

static int kfs_find_free_slot(void) {
    int i;

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (!g_kfs_files[i].used) {
            return i;
        }
    }

    return -1;
}

void kfs_init(void) {
    kfs_clear();
}

void kfs_clear(void) {
    int i;

    kfs_lock();

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (g_kfs_files[i].data) {
            kfree(g_kfs_files[i].data);
        }

        g_kfs_files[i].used = 0;
        g_kfs_files[i].name[0] = 0;
        g_kfs_files[i].data = NULL;
        g_kfs_files[i].size = 0;
    }

    kfs_unlock();
}

int kfs_write(const char* name, const char* content) {
    int idx;
    size_t len;
    char* new_data;

    if (!kfs_is_valid_name(name)) {
        return 0;
    }

    if (!content) {
        content = "";
    }

    len = kfs_strlen(content);

    if (len > KFS_MAX_FILE_SIZE) {
        return 0;
    }

    new_data = (char*)kmalloc(len + 1);

    if (!new_data) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        new_data[i] = content[i];
    }
    new_data[len] = 0;

    kfs_lock();

    idx = kfs_find_index(name);

    if (idx < 0) {
        idx = kfs_find_free_slot();
        if (idx < 0) {
            kfs_unlock();
            kfree(new_data);
            return 0;
        }

        g_kfs_files[idx].used = 1;
        kfs_strncpy0(g_kfs_files[idx].name, name, sizeof(g_kfs_files[idx].name));
        g_kfs_files[idx].data = NULL;
        g_kfs_files[idx].size = 0;
    }

    if (g_kfs_files[idx].data) {
        kfree(g_kfs_files[idx].data);
    }

    g_kfs_files[idx].data = new_data;
    g_kfs_files[idx].size = len;

    kfs_unlock();
    kfs_persist_save();
    return 1;
}

int kfs_append(const char* name, const char* content) {
    int idx;
    size_t old_size;
    size_t add_size;
    size_t new_size;
    char* resized;

    if (!kfs_is_valid_name(name)) {
        return 0;
    }

    if (!content || !content[0]) {
        return 1;
    }

    kfs_lock();

    idx = kfs_find_index(name);
    if (idx < 0) {
        kfs_unlock();
        return kfs_write(name, content);
    }

    old_size = g_kfs_files[idx].size;
    add_size = kfs_strlen(content);

    if (add_size > KFS_MAX_FILE_SIZE || old_size > KFS_MAX_FILE_SIZE) {
        kfs_unlock();
        return 0;
    }

    if (old_size > KFS_MAX_FILE_SIZE - add_size) {
        kfs_unlock();
        return 0;
    }

    new_size = old_size + add_size;

    resized = (char*)krealloc(g_kfs_files[idx].data, new_size + 1);
    if (!resized) {
        kfs_unlock();
        return 0;
    }

    for (size_t i = 0; i < add_size; i++) {
        resized[old_size + i] = content[i];
    }

    resized[new_size] = 0;
    g_kfs_files[idx].data = resized;
    g_kfs_files[idx].size = new_size;

    kfs_unlock();
    kfs_persist_save();
    return 1;
}

const char* kfs_read(const char* name) {
    int idx;

    if (!kfs_is_valid_name(name)) {
        return NULL;
    }

    idx = kfs_find_index(name);
    if (idx < 0) {
        return NULL;
    }

    return g_kfs_files[idx].data ? g_kfs_files[idx].data : "";
}

int kfs_delete(const char* name) {
    int idx;

    if (!kfs_is_valid_name(name)) {
        return 0;
    }

    kfs_lock();

    idx = kfs_find_index(name);
    if (idx < 0) {
        kfs_unlock();
        return 0;
    }

    if (g_kfs_files[idx].data) {
        kfree(g_kfs_files[idx].data);
    }

    g_kfs_files[idx].used = 0;
    g_kfs_files[idx].name[0] = 0;
    g_kfs_files[idx].data = NULL;
    g_kfs_files[idx].size = 0;

    kfs_unlock();
    kfs_persist_save();
    return 1;
}

int kfs_exists(const char* name) {
    if (!kfs_is_valid_name(name)) {
        return 0;
    }

    return kfs_find_index(name) >= 0;
}

size_t kfs_size(const char* name) {
    int idx;

    if (!kfs_is_valid_name(name)) {
        return 0;
    }

    idx = kfs_find_index(name);
    if (idx < 0) {
        return 0;
    }

    return g_kfs_files[idx].size;
}

size_t kfs_count(void) {
    size_t count = 0;
    int i;

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (g_kfs_files[i].used) {
            count++;
        }
    }

    return count;
}

const char* kfs_name_at(size_t index) {
    size_t current = 0;
    int i;

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (!g_kfs_files[i].used) {
            continue;
        }

        if (current == index) {
            return g_kfs_files[i].name;
        }

        current++;
    }

    return NULL;
}

/* --- Persistence via ATA secondary disk --- */

/*
 * Disk layout (all little-endian):
 *
 * Sector 0 (header):
 *   [0..3]   magic "PLFS"
 *   [4..7]   version (uint32 = 1)
 *   [8..11]  file_count (uint32)
 *   [12..511] reserved
 *
 * For each file (sequentially after header):
 *   Entry sector:
 *     [0..47]  name (null-terminated, 48 bytes)
 *     [48..51] size (uint32)
 *     [52..511] reserved
 *   Data sectors:
 *     ceil(size / 512) sectors of file content
 */

static void kfs_put_u32(uint8_t* buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static uint32_t kfs_get_u32(const uint8_t* buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

int kfs_persist_available(void) {
    return ata_is_available();
}

int kfs_persist_save(void) {
    uint8_t sector[KFS_SECTOR_SIZE];
    uint32_t lba = 0;
    uint32_t file_count = 0;
    int i;

    if (!ata_is_available()) {
        return 0;
    }

    /* Count files */
    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (g_kfs_files[i].used) {
            file_count++;
        }
    }

    /* Write header sector */
    for (i = 0; i < (int)KFS_SECTOR_SIZE; i++) {
        sector[i] = 0;
    }

    sector[0] = KFS_DISK_MAGIC_0;
    sector[1] = KFS_DISK_MAGIC_1;
    sector[2] = KFS_DISK_MAGIC_2;
    sector[3] = KFS_DISK_MAGIC_3;
    kfs_put_u32(&sector[4], KFS_DISK_VERSION);
    kfs_put_u32(&sector[8], file_count);

    if (!ata_write_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
        serial_print("[kfs] persist save: header write fail\r\n");
        return 0;
    }

    lba = 1;

    /* Write each file */
    for (i = 0; i < KFS_MAX_FILES; i++) {
        uint32_t fsize;
        uint32_t data_sectors;
        uint32_t s;
        int j;

        if (!g_kfs_files[i].used) {
            continue;
        }

        fsize = (uint32_t)g_kfs_files[i].size;

        /* Write entry sector */
        for (j = 0; j < (int)KFS_SECTOR_SIZE; j++) {
            sector[j] = 0;
        }

        kfs_strncpy0((char*)sector, g_kfs_files[i].name, KFS_MAX_NAME);
        kfs_put_u32(&sector[48], fsize);

        if (!ata_write_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
            serial_print("[kfs] persist save: entry write fail\r\n");
            return 0;
        }

        lba++;

        /* Write data sectors */
        data_sectors = (fsize + KFS_SECTOR_SIZE - 1) / KFS_SECTOR_SIZE;

        for (s = 0; s < data_sectors; s++) {
            uint32_t offset = s * KFS_SECTOR_SIZE;
            uint32_t remain = fsize - offset;
            uint32_t copy_len = remain < KFS_SECTOR_SIZE ? remain : KFS_SECTOR_SIZE;

            for (j = 0; j < (int)KFS_SECTOR_SIZE; j++) {
                sector[j] = 0;
            }

            if (g_kfs_files[i].data) {
                for (j = 0; j < (int)copy_len; j++) {
                    sector[j] = (uint8_t)g_kfs_files[i].data[offset + j];
                }
            }

            if (!ata_write_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
                serial_print("[kfs] persist save: data write fail\r\n");
                return 0;
            }

            lba++;
        }
    }

    serial_print("[kfs] persist save: OK\r\n");
    return 1;
}

int kfs_persist_load(void) {
    uint8_t sector[KFS_SECTOR_SIZE];
    uint32_t lba = 0;
    uint32_t file_count;
    uint32_t f;

    if (!ata_is_available()) {
        return 0;
    }

    /* Read header */
    if (!ata_read_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
        serial_print("[kfs] persist load: header read fail\r\n");
        return 0;
    }

    /* Validate magic */
    if (sector[0] != KFS_DISK_MAGIC_0 || sector[1] != KFS_DISK_MAGIC_1 ||
        sector[2] != KFS_DISK_MAGIC_2 || sector[3] != KFS_DISK_MAGIC_3) {
        serial_print("[kfs] persist load: no PLFS header\r\n");
        return 0;
    }

    if (kfs_get_u32(&sector[4]) != KFS_DISK_VERSION) {
        serial_print("[kfs] persist load: version mismatch\r\n");
        return 0;
    }

    file_count = kfs_get_u32(&sector[8]);
    if (file_count > KFS_MAX_FILES) {
        serial_print("[kfs] persist load: too many files\r\n");
        return 0;
    }

    lba = 1;

    for (f = 0; f < file_count; f++) {
        char name[KFS_MAX_NAME];
        uint32_t fsize;
        uint32_t data_sectors;
        char* file_data;
        uint32_t s;
        int j;

        /* Read entry sector */
        if (!ata_read_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
            serial_print("[kfs] persist load: entry read fail\r\n");
            return 0;
        }

        lba++;

        kfs_strncpy0(name, (const char*)sector, KFS_MAX_NAME);
        fsize = kfs_get_u32(&sector[48]);

        if (fsize > KFS_MAX_FILE_SIZE) {
            serial_print("[kfs] persist load: file too large\r\n");
            return 0;
        }

        data_sectors = (fsize + KFS_SECTOR_SIZE - 1) / KFS_SECTOR_SIZE;

        file_data = (char*)kmalloc(fsize + 1);
        if (!file_data && fsize > 0) {
            serial_print("[kfs] persist load: alloc fail\r\n");
            return 0;
        }

        for (s = 0; s < data_sectors; s++) {
            uint32_t offset = s * KFS_SECTOR_SIZE;
            uint32_t remain = fsize - offset;
            uint32_t copy_len = remain < KFS_SECTOR_SIZE ? remain : KFS_SECTOR_SIZE;

            if (!ata_read_sectors(lba, 1, sector, KFS_SECTOR_SIZE)) {
                serial_print("[kfs] persist load: data read fail\r\n");
                if (file_data) {
                    kfree(file_data);
                }
                return 0;
            }

            lba++;

            if (file_data) {
                for (j = 0; j < (int)copy_len; j++) {
                    file_data[offset + j] = (char)sector[j];
                }
            }
        }

        if (file_data) {
            file_data[fsize] = 0;
        }

        /* Store in kfs */
        {
            int slot = kfs_find_free_slot();
            if (slot < 0) {
                serial_print("[kfs] persist load: no free slot\r\n");
                if (file_data) {
                    kfree(file_data);
                }
                return 0;
            }

            g_kfs_files[slot].used = 1;
            kfs_strncpy0(g_kfs_files[slot].name, name, sizeof(g_kfs_files[slot].name));
            g_kfs_files[slot].data = file_data;
            g_kfs_files[slot].size = fsize;
        }
    }

    serial_print("[kfs] persist load: OK\r\n");
    return 1;
}
