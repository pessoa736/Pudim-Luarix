#include "ksecurity.h"

#define KSECURITY_NAME_CAP (KSECURITY_MAX_USERNAME)

typedef struct {
    int used;
    unsigned int uid;
    unsigned int gid;
    uint32_t capabilities;
    char name[KSECURITY_NAME_CAP];
} ksecurity_user_t;

static ksecurity_user_t g_ksecurity_users[KSECURITY_MAX_USERS];

static void ksecurity_strncpy(char* dst, unsigned int dst_size, const char* src) {
    unsigned int i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = 0;
        return;
    }

    while (src[i] && (i + 1) < dst_size) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

void ksecurity_init(void) {
    unsigned int i;

    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        g_ksecurity_users[i].used = 0;
        g_ksecurity_users[i].uid = 0;
        g_ksecurity_users[i].gid = 0;
        g_ksecurity_users[i].capabilities = 0;
        g_ksecurity_users[i].name[0] = 0;
    }

    /* Create root user with all capabilities */
    ksecurity_create_user("root", KSECURITY_ROOT_UID, KSECURITY_ROOT_GID,
                          KSECURITY_CAP_ALL);
}

int ksecurity_create_user(const char* name, unsigned int uid, unsigned int gid,
                          uint32_t capabilities) {
    unsigned int i;

    if (!name || name[0] == 0) {
        return 0;
    }

    /* Check if uid already exists */
    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        if (g_ksecurity_users[i].used && g_ksecurity_users[i].uid == uid) {
            return 0;
        }
    }

    /* Find free slot */
    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        if (!g_ksecurity_users[i].used) {
            g_ksecurity_users[i].used = 1;
            g_ksecurity_users[i].uid = uid;
            g_ksecurity_users[i].gid = gid;
            g_ksecurity_users[i].capabilities = capabilities;
            ksecurity_strncpy(g_ksecurity_users[i].name, KSECURITY_NAME_CAP, name);
            return 1;
        }
    }

    return 0;
}

int ksecurity_user_exists(unsigned int uid) {
    unsigned int i;

    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        if (g_ksecurity_users[i].used && g_ksecurity_users[i].uid == uid) {
            return 1;
        }
    }

    return 0;
}

int ksecurity_get_user_name(unsigned int uid, char* out, unsigned int out_size) {
    unsigned int i;

    if (!out || out_size == 0) {
        return 0;
    }

    out[0] = 0;

    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        if (g_ksecurity_users[i].used && g_ksecurity_users[i].uid == uid) {
            ksecurity_strncpy(out, out_size, g_ksecurity_users[i].name);
            return 1;
        }
    }

    return 0;
}

uint32_t ksecurity_get_capabilities(unsigned int uid) {
    unsigned int i;

    for (i = 0; i < KSECURITY_MAX_USERS; i++) {
        if (g_ksecurity_users[i].used && g_ksecurity_users[i].uid == uid) {
            return g_ksecurity_users[i].capabilities;
        }
    }

    return 0;
}

int ksecurity_check_capability(unsigned int uid, uint32_t cap) {
    uint32_t caps = ksecurity_get_capabilities(uid);
    return (caps & cap) == cap;
}

int ksecurity_check_file_permission(unsigned int caller_uid, unsigned int caller_gid,
                                    unsigned int owner_uid, unsigned int owner_gid,
                                    unsigned int mode, unsigned int access) {
    unsigned int perm;

    /* Root always has access */
    if (caller_uid == KSECURITY_ROOT_UID) {
        return 1;
    }

    /* Check owner permissions */
    if (caller_uid == owner_uid) {
        perm = KSECURITY_MODE_OWNER(mode);
    } else if (caller_gid == owner_gid) {
        /* Check group permissions */
        perm = KSECURITY_MODE_GROUP(mode);
    } else {
        /* Check other permissions */
        perm = KSECURITY_MODE_OTHER(mode);
    }

    /* Map access request to permission bits */
    if ((access & KSECURITY_ACCESS_R) && !(perm & KSECURITY_PERM_R)) {
        return 0;
    }

    if ((access & KSECURITY_ACCESS_W) && !(perm & KSECURITY_PERM_W)) {
        return 0;
    }

    if ((access & KSECURITY_ACCESS_X) && !(perm & KSECURITY_PERM_X)) {
        return 0;
    }

    return 1;
}
