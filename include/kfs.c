#include "kfs.h"
#include "kheap.h"

#define KFS_MAX_FILES 32
#define KFS_MAX_NAME  48
#define KFS_MAX_FILE_SIZE 65536
#define KFS_MAX_TOTAL_SIZE (KFS_MAX_FILE_SIZE * KFS_MAX_FILES)

typedef struct kfs_file {
    int used;
    char name[KFS_MAX_NAME];
    char* data;
    size_t size;
} kfs_file_t;

static kfs_file_t g_kfs_files[KFS_MAX_FILES];

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

    for (i = 0; i < KFS_MAX_FILES; i++) {
        if (g_kfs_files[i].data) {
            kfree(g_kfs_files[i].data);
        }

        g_kfs_files[i].used = 0;
        g_kfs_files[i].name[0] = 0;
        g_kfs_files[i].data = NULL;
        g_kfs_files[i].size = 0;
    }
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

    idx = kfs_find_index(name);

    if (idx < 0) {
        idx = kfs_find_free_slot();
        if (idx < 0) {
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

    idx = kfs_find_index(name);
    if (idx < 0) {
        return kfs_write(name, content);
    }

    old_size = g_kfs_files[idx].size;
    add_size = kfs_strlen(content);

    if (add_size > KFS_MAX_FILE_SIZE || old_size > KFS_MAX_FILE_SIZE) {
        return 0;
    }

    if (old_size > KFS_MAX_FILE_SIZE - add_size) {
        return 0;
    }

    new_size = old_size + add_size;

    resized = (char*)krealloc(g_kfs_files[idx].data, new_size + 1);
    if (!resized) {
        return 0;
    }

    for (size_t i = 0; i < add_size; i++) {
        resized[old_size + i] = content[i];
    }

    resized[new_size] = 0;
    g_kfs_files[idx].data = resized;
    g_kfs_files[idx].size = new_size;

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

    idx = kfs_find_index(name);
    if (idx < 0) {
        return 0;
    }

    if (g_kfs_files[idx].data) {
        kfree(g_kfs_files[idx].data);
    }

    g_kfs_files[idx].used = 0;
    g_kfs_files[idx].name[0] = 0;
    g_kfs_files[idx].data = NULL;
    g_kfs_files[idx].size = 0;

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
