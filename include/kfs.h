#ifndef KFS_H
#define KFS_H

#include "stddef.h"

void kfs_init(void);
void kfs_clear(void);

int kfs_write(const char* name, const char* content);
int kfs_append(const char* name, const char* content);
const char* kfs_read(const char* name);
int kfs_delete(const char* name);
int kfs_exists(const char* name);

size_t kfs_size(const char* name);
size_t kfs_count(void);
const char* kfs_name_at(size_t index);

/* File permissions */
int kfs_chmod(const char* name, unsigned int mode);
int kfs_chown(const char* name, unsigned int uid, unsigned int gid);
unsigned int kfs_get_mode(const char* name);
unsigned int kfs_get_owner(const char* name);
unsigned int kfs_get_group(const char* name);

/* Persistence via ATA secondary disk */
int kfs_persist_load(void);
int kfs_persist_save(void);
int kfs_persist_available(void);

#endif
