#ifndef PLFS_H
#define PLFS_H

#include "../fs_common.h"

#define PLFS_MAGIC_0 'P'
#define PLFS_MAGIC_1 'L'
#define PLFS_MAGIC_2 'F'
#define PLFS_MAGIC_3 'S'
#define PLFS_VERSION     1u
#define PLFS_MAX_FILES  32u
#define PLFS_MAX_NAME   48u
#define PLFS_MAX_FILE_SIZE 65536u
#define PLFS_MAX_SECTORS 4200u

/* Detect PLFS at the given LBA offset on the ATA disk */
int plfs_detect(uint32_t lba_start);

/* Get filesystem info */
int plfs_info(uint32_t lba_start, fs_info_t* out);

/* List all files (calls cb for each) */
int plfs_list(uint32_t lba_start, fs_list_cb cb, void* ctx);

/* Read a file by name into a buffer. Returns bytes read, 0 on failure. */
uint32_t plfs_read(uint32_t lba_start, const char* name,
                   void* buf, uint32_t bufsize);

/* Write a file by name from a buffer. Returns 1 on success. */
int plfs_write(uint32_t lba_start, const char* name,
               const void* data, uint32_t size);

/* Delete a file by name. Returns 1 on success. */
int plfs_delete(uint32_t lba_start, const char* name);

/* Format disk region with empty PLFS header. */
int plfs_format(uint32_t lba_start);

#endif
