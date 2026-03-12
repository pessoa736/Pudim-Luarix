#ifndef KSECURITY_H
#define KSECURITY_H

#include "utypes.h"

#define KSECURITY_MAX_USERS    8
#define KSECURITY_MAX_USERNAME 32

/* Root user identity */
#define KSECURITY_ROOT_UID 0
#define KSECURITY_ROOT_GID 0

/* Default file permission mode: rw-r--r-- */
#define KSECURITY_DEFAULT_MODE 0644u

/* Capability flags */
#define KSECURITY_CAP_SETUID        (1u << 0)
#define KSECURITY_CAP_CHMOD         (1u << 1)
#define KSECURITY_CAP_CHOWN         (1u << 2)
#define KSECURITY_CAP_KILL          (1u << 3)
#define KSECURITY_CAP_FS_ADMIN      (1u << 4)
#define KSECURITY_CAP_PROCESS_ADMIN (1u << 5)

/* All capabilities combined */
#define KSECURITY_CAP_ALL ( \
    KSECURITY_CAP_SETUID | KSECURITY_CAP_CHMOD | KSECURITY_CAP_CHOWN | \
    KSECURITY_CAP_KILL | KSECURITY_CAP_FS_ADMIN | KSECURITY_CAP_PROCESS_ADMIN )

/* Permission bits (octal-style) */
#define KSECURITY_PERM_R 4u
#define KSECURITY_PERM_W 2u
#define KSECURITY_PERM_X 1u

/* Extract permission triplets from a 9-bit mode */
#define KSECURITY_MODE_OWNER(mode) (((mode) >> 6) & 7u)
#define KSECURITY_MODE_GROUP(mode) (((mode) >> 3) & 7u)
#define KSECURITY_MODE_OTHER(mode) ((mode) & 7u)

/* Access type for permission checks */
#define KSECURITY_ACCESS_R 1u
#define KSECURITY_ACCESS_W 2u
#define KSECURITY_ACCESS_X 4u

void ksecurity_init(void);

int ksecurity_create_user(const char* name, unsigned int uid, unsigned int gid,
                          uint32_t capabilities);
int ksecurity_user_exists(unsigned int uid);
int ksecurity_get_user_name(unsigned int uid, char* out, unsigned int out_size);
uint32_t ksecurity_get_capabilities(unsigned int uid);

int ksecurity_check_capability(unsigned int uid, uint32_t cap);

int ksecurity_check_file_permission(unsigned int caller_uid, unsigned int caller_gid,
                                    unsigned int owner_uid, unsigned int owner_gid,
                                    unsigned int mode, unsigned int access);

#endif
