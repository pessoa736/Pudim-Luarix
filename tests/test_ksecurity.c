/* Unit tests for ksecurity.c and kfs permission extensions */
#include "test_framework.h"
#include <stdint.h>

#include "../include/utypes.h"
#include "../include/stddef.h"

/* Provide __kernel_end for kheap */
uint8_t __kernel_end;
#include "../include/kheap.c"

/* Stub ATA persistence */
int ata_init(void) { return 0; }
int ata_read_sectors(uint32_t lba, uint32_t count, void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size; return 0;
}
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buffer, uint32_t buffer_size) {
    (void)lba; (void)count; (void)buffer; (void)buffer_size; return 0;
}
int ata_is_available(void) { return 0; }

/* Stub serial */
void serial_print(const char* s) { (void)s; }

/* Include sources under test */
#include "../include/ksecurity.c"
#include "../include/kfs.c"

/* Init heap with a static buffer */
static char fake_heap[0x200000] __attribute__((aligned(4096)));

static void setup(void) {
    heap_region_start = (size_t)fake_heap;
    heap_region_end = (size_t)fake_heap + 0x100000;
    heap_start = (block_header_t*)fake_heap;
    heap_start->size = (0x100000 - sizeof(block_header_t)) & ~(HEAP_ALIGNMENT - 1);
    heap_start->free = 1;
    heap_start->magic = BLOCK_MAGIC_FREE;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    g_heap_lock = 0;
    ksecurity_init();
    kfs_init();
}

/* --- ksecurity core tests --- */

static void test_init_creates_root(void) {
    TEST_SUITE("ksecurity init");

    setup();

    ASSERT(ksecurity_user_exists(0), "root user exists after init");

    {
        char name[32];
        int ok = ksecurity_get_user_name(0, name, sizeof(name));
        ASSERT(ok, "get_user_name returns 1 for root");
        ASSERT_STR_EQ(name, "root", "root user name is 'root'");
    }
}

static void test_create_user(void) {
    TEST_SUITE("ksecurity create_user");

    setup();

    ASSERT(ksecurity_create_user("alice", 1, 1, 0), "create user alice");
    ASSERT(ksecurity_user_exists(1), "alice exists");

    {
        char name[32];
        ksecurity_get_user_name(1, name, sizeof(name));
        ASSERT_STR_EQ(name, "alice", "alice name correct");
    }

    ASSERT(!ksecurity_create_user("bob", 1, 2, 0), "duplicate uid rejected");
    ASSERT(!ksecurity_create_user(NULL, 5, 5, 0), "null name rejected");
    ASSERT(!ksecurity_create_user("", 5, 5, 0), "empty name rejected");
}

static void test_user_not_exists(void) {
    TEST_SUITE("ksecurity user_not_exists");

    setup();

    ASSERT(!ksecurity_user_exists(99), "nonexistent uid returns false");

    {
        char name[32];
        name[0] = 'X';
        int ok = ksecurity_get_user_name(99, name, sizeof(name));
        ASSERT(!ok, "get_user_name fails for nonexistent uid");
        ASSERT_EQ(name[0], 0, "output zeroed on failure");
    }
}

static void test_max_users(void) {
    TEST_SUITE("ksecurity max users");

    setup();
    /* root already uses slot 0 */

    ASSERT(ksecurity_create_user("u1", 1, 1, 0), "create user 1");
    ASSERT(ksecurity_create_user("u2", 2, 2, 0), "create user 2");
    ASSERT(ksecurity_create_user("u3", 3, 3, 0), "create user 3");
    ASSERT(ksecurity_create_user("u4", 4, 4, 0), "create user 4");
    ASSERT(ksecurity_create_user("u5", 5, 5, 0), "create user 5");
    ASSERT(ksecurity_create_user("u6", 6, 6, 0), "create user 6");
    ASSERT(ksecurity_create_user("u7", 7, 7, 0), "create user 7");
    ASSERT(!ksecurity_create_user("u8", 8, 8, 0), "9th user rejected (max 8)");
}

/* --- Capabilities tests --- */

static void test_capabilities(void) {
    TEST_SUITE("ksecurity capabilities");

    setup();

    /* Root has all capabilities */
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_SETUID), "root has SETUID");
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_CHMOD), "root has CHMOD");
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_CHOWN), "root has CHOWN");
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_KILL), "root has KILL");
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_FS_ADMIN), "root has FS_ADMIN");
    ASSERT(ksecurity_check_capability(0, KSECURITY_CAP_PROCESS_ADMIN), "root has PROCESS_ADMIN");

    /* Create user with limited caps */
    ksecurity_create_user("limited", 10, 10, KSECURITY_CAP_CHMOD);
    ASSERT(ksecurity_check_capability(10, KSECURITY_CAP_CHMOD), "limited has CHMOD");
    ASSERT(!ksecurity_check_capability(10, KSECURITY_CAP_SETUID), "limited lacks SETUID");
    ASSERT(!ksecurity_check_capability(10, KSECURITY_CAP_KILL), "limited lacks KILL");

    /* Unknown uid has no caps */
    ASSERT_EQ(ksecurity_get_capabilities(99), 0u, "unknown uid has 0 caps");
    ASSERT(!ksecurity_check_capability(99, KSECURITY_CAP_SETUID), "unknown uid fails cap check");
}

/* --- Permission check tests --- */

static void test_file_permissions_owner(void) {
    TEST_SUITE("ksecurity file perms - owner");

    setup();

    /* mode 0644: owner=rw, group=r, other=r */
    ASSERT(ksecurity_check_file_permission(1, 1, 1, 1, 0644, KSECURITY_ACCESS_R),
           "owner can read (0644)");
    ASSERT(ksecurity_check_file_permission(1, 1, 1, 1, 0644, KSECURITY_ACCESS_W),
           "owner can write (0644)");
    ASSERT(!ksecurity_check_file_permission(1, 1, 1, 1, 0644, KSECURITY_ACCESS_X),
           "owner cannot exec (0644)");

    /* mode 0755: owner=rwx */
    ASSERT(ksecurity_check_file_permission(1, 1, 1, 1, 0755, KSECURITY_ACCESS_X),
           "owner can exec (0755)");
}

static void test_file_permissions_group(void) {
    TEST_SUITE("ksecurity file perms - group");

    setup();

    /* uid=2, gid=1, owner_uid=1, owner_gid=1 => group match */
    ASSERT(ksecurity_check_file_permission(2, 1, 1, 1, 0644, KSECURITY_ACCESS_R),
           "group can read (0644)");
    ASSERT(!ksecurity_check_file_permission(2, 1, 1, 1, 0644, KSECURITY_ACCESS_W),
           "group cannot write (0644)");

    /* mode 0674: group=rwx */
    ASSERT(ksecurity_check_file_permission(2, 1, 1, 1, 0674, KSECURITY_ACCESS_W),
           "group can write (0674)");
    ASSERT(ksecurity_check_file_permission(2, 1, 1, 1, 0674, KSECURITY_ACCESS_X),
           "group can exec (0674)");
}

static void test_file_permissions_other(void) {
    TEST_SUITE("ksecurity file perms - other");

    setup();

    /* uid=3, gid=3, owner_uid=1, owner_gid=1 => other */
    ASSERT(ksecurity_check_file_permission(3, 3, 1, 1, 0644, KSECURITY_ACCESS_R),
           "other can read (0644)");
    ASSERT(!ksecurity_check_file_permission(3, 3, 1, 1, 0644, KSECURITY_ACCESS_W),
           "other cannot write (0644)");
    ASSERT(!ksecurity_check_file_permission(3, 3, 1, 1, 0600, KSECURITY_ACCESS_R),
           "other cannot read (0600)");
}

static void test_file_permissions_root_bypass(void) {
    TEST_SUITE("ksecurity file perms - root bypass");

    setup();

    /* Root (uid=0) can access anything regardless of mode */
    ASSERT(ksecurity_check_file_permission(0, 0, 1, 1, 0000, KSECURITY_ACCESS_R),
           "root can read (0000)");
    ASSERT(ksecurity_check_file_permission(0, 0, 1, 1, 0000, KSECURITY_ACCESS_W),
           "root can write (0000)");
    ASSERT(ksecurity_check_file_permission(0, 0, 1, 1, 0000, KSECURITY_ACCESS_X),
           "root can exec (0000)");
}

/* --- kfs permission field tests --- */

static void test_kfs_default_permissions(void) {
    TEST_SUITE("kfs default permissions");

    setup();

    kfs_write("testfile", "hello");

    ASSERT_EQ(kfs_get_mode("testfile"), 0644u, "new file has mode 0644");
    ASSERT_EQ(kfs_get_owner("testfile"), 0u, "new file owned by uid 0");
    ASSERT_EQ(kfs_get_group("testfile"), 0u, "new file owned by gid 0");
}

static void test_kfs_chmod(void) {
    TEST_SUITE("kfs chmod");

    setup();

    kfs_write("cfile", "data");

    ASSERT(kfs_chmod("cfile", 0755), "chmod to 0755");
    ASSERT_EQ(kfs_get_mode("cfile"), 0755u, "mode is 0755 after chmod");

    ASSERT(kfs_chmod("cfile", 0600), "chmod to 0600");
    ASSERT_EQ(kfs_get_mode("cfile"), 0600u, "mode is 0600 after chmod");

    ASSERT(!kfs_chmod("cfile", 01000), "mode > 0777 rejected");
    ASSERT(!kfs_chmod("nonexistent", 0644), "chmod on nonexistent fails");
}

static void test_kfs_chown(void) {
    TEST_SUITE("kfs chown");

    setup();

    kfs_write("ofile", "data");

    ASSERT(kfs_chown("ofile", 5, 10), "chown to 5:10");
    ASSERT_EQ(kfs_get_owner("ofile"), 5u, "owner is 5");
    ASSERT_EQ(kfs_get_group("ofile"), 10u, "group is 10");

    ASSERT(!kfs_chown("nonexistent", 1, 1), "chown on nonexistent fails");
}

static void test_kfs_delete_resets_perms(void) {
    TEST_SUITE("kfs delete resets perms");

    setup();

    kfs_write("dfile", "data");
    kfs_chown("dfile", 5, 10);
    kfs_chmod("dfile", 0777);
    kfs_delete("dfile");

    /* Re-create should get defaults */
    kfs_write("dfile", "new");
    ASSERT_EQ(kfs_get_mode("dfile"), 0644u, "recreated file has default mode");
    ASSERT_EQ(kfs_get_owner("dfile"), 0u, "recreated file has default owner");
}

int main(void) {
    test_init_creates_root();
    test_create_user();
    test_user_not_exists();
    test_max_users();
    test_capabilities();
    test_file_permissions_owner();
    test_file_permissions_group();
    test_file_permissions_other();
    test_file_permissions_root_bypass();
    test_kfs_default_permissions();
    test_kfs_chmod();
    test_kfs_chown();
    test_kfs_delete_resets_perms();

    TEST_RESULTS();
}
