/* Unit tests for libc_shim.c */
#include "test_framework.h"

/* We test on the host, so we use standard headers for comparison,
   but call the kernel's implementations via renamed symbols. */

/* Include the shim source directly to test its functions */
/* First, undefine any conflicting symbols */
#define abs     k_abs
#define memcpy  k_memcpy
#define memset  k_memset
#define memcmp  k_memcmp
#define strlen  k_strlen
#define strcpy  k_strcpy
#define strcmp   k_strcmp
#define strncmp k_strncmp
#define strchr  k_strchr
#define strpbrk k_strpbrk
#define strspn  k_strspn
#define strcoll k_strcoll
#define strtod  k_strtod
#define floor   k_floor
#define fmod    k_fmod
#define pow     k_pow
#define ldexp   k_ldexp
#define frexp   k_frexp
#define localeconv k_localeconv
#define snprintf k_snprintf
#define abort   k_abort

/* Provide utypes for the shim */
typedef unsigned char  uint8_t_k;
typedef unsigned long  uint64_t_k;

/* Minimal stubs the shim needs */
#include "../include/utypes.h"
#include "../include/stddef.h"

/* Now include the actual shim source */
#include "../include/libc_shim.c"

#undef abs
#undef memcpy
#undef memset
#undef memcmp
#undef strlen
#undef strcpy
#undef strcmp
#undef strncmp
#undef strchr
#undef strpbrk
#undef strspn
#undef strcoll
#undef strtod
#undef floor
#undef fmod
#undef pow
#undef ldexp
#undef frexp
#undef localeconv
#undef snprintf
#undef abort

static void test_abs(void) {
    TEST_SUITE("libc_shim: abs");
    ASSERT_EQ(k_abs(5), 5, "abs(5) == 5");
    ASSERT_EQ(k_abs(-5), 5, "abs(-5) == 5");
    ASSERT_EQ(k_abs(0), 0, "abs(0) == 0");
}

static void test_memcpy(void) {
    char src[] = "hello";
    char dst[16] = {0};

    TEST_SUITE("libc_shim: memcpy");
    k_memcpy(dst, src, 6);
    ASSERT(memcmp(dst, "hello", 6) == 0, "memcpy copies correctly");
}

static void test_memset(void) {
    char buf[8];

    TEST_SUITE("libc_shim: memset");
    k_memset(buf, 'A', 5);
    buf[5] = 0;
    ASSERT(memcmp(buf, "AAAAA", 5) == 0, "memset fills correctly");
}

static void test_memcmp(void) {
    TEST_SUITE("libc_shim: memcmp");
    ASSERT_EQ(k_memcmp("abc", "abc", 3), 0, "equal buffers");
    ASSERT(k_memcmp("abc", "abd", 3) < 0, "abc < abd");
    ASSERT(k_memcmp("abd", "abc", 3) > 0, "abd > abc");
}

static void test_strlen(void) {
    TEST_SUITE("libc_shim: strlen");
    ASSERT_EQ(k_strlen("hello"), 5, "strlen('hello') == 5");
    ASSERT_EQ(k_strlen(""), 0, "strlen('') == 0");
    ASSERT_EQ(k_strlen("a"), 1, "strlen('a') == 1");
}

static void test_strcpy(void) {
    char dst[16];

    TEST_SUITE("libc_shim: strcpy");
    k_strcpy(dst, "test");
    ASSERT(strcmp(dst, "test") == 0, "strcpy copies string");
}

static void test_strcmp(void) {
    TEST_SUITE("libc_shim: strcmp");
    ASSERT_EQ(k_strcmp("abc", "abc"), 0, "equal strings");
    ASSERT(k_strcmp("abc", "abd") < 0, "abc < abd");
    ASSERT(k_strcmp("abd", "abc") > 0, "abd > abc");
    ASSERT(k_strcmp("ab", "abc") < 0, "ab < abc (shorter)");
}

static void test_strncmp(void) {
    TEST_SUITE("libc_shim: strncmp");
    ASSERT_EQ(k_strncmp("abcX", "abcY", 3), 0, "equal within n");
    ASSERT(k_strncmp("abcX", "abcY", 4) != 0, "different at n");
}

static void test_strchr(void) {
    TEST_SUITE("libc_shim: strchr");
    ASSERT(k_strchr("hello", 'l') != NULL, "found 'l'");
    ASSERT_EQ(*k_strchr("hello", 'l'), 'l', "points to 'l'");
    ASSERT_NULL(k_strchr("hello", 'z'), "not found returns NULL");
    ASSERT_NOT_NULL(k_strchr("hello", 0), "NUL terminator found");
}

static void test_strpbrk(void) {
    TEST_SUITE("libc_shim: strpbrk");
    ASSERT_NOT_NULL(k_strpbrk("hello", "lo"), "found 'l' or 'o'");
    ASSERT_EQ(*k_strpbrk("hello", "lo"), 'l', "first match is 'l'");
    ASSERT_NULL(k_strpbrk("hello", "xyz"), "none found");
}

static void test_strspn(void) {
    TEST_SUITE("libc_shim: strspn");
    ASSERT_EQ(k_strspn("aabbc", "ab"), 4, "4 chars in accept set");
    ASSERT_EQ(k_strspn("xyz", "ab"), 0, "0 chars in accept set");
}

static void test_floor(void) {
    TEST_SUITE("libc_shim: floor");
    ASSERT_EQ((int)k_floor(2.7), 2, "floor(2.7) == 2");
    ASSERT_EQ((int)k_floor(-2.3), -3, "floor(-2.3) == -3");
    ASSERT_EQ((int)k_floor(3.0), 3, "floor(3.0) == 3");
}

static void test_fmod(void) {
    TEST_SUITE("libc_shim: fmod");
    double r = k_fmod(5.5, 2.0);
    ASSERT(r > 1.49 && r < 1.51, "fmod(5.5, 2.0) ~= 1.5");
    ASSERT_EQ((int)k_fmod(0.0, 1.0), 0, "fmod(0,1) == 0");
}

static void test_pow(void) {
    TEST_SUITE("libc_shim: pow");
    ASSERT_EQ((int)k_pow(2.0, 10.0), 1024, "2^10 == 1024");
    ASSERT_EQ((int)k_pow(3.0, 0.0), 1, "3^0 == 1");
    ASSERT_EQ((int)k_pow(5.0, 1.0), 5, "5^1 == 5");
}

static void test_strtod(void) {
    char* end;
    double v;

    TEST_SUITE("libc_shim: strtod");
    v = k_strtod("3.14", &end);
    ASSERT(v > 3.13 && v < 3.15, "strtod('3.14') ~= 3.14");
    ASSERT_EQ(*end, '\0', "end points to NUL");

    v = k_strtod("-42", &end);
    ASSERT_EQ((int)v, -42, "strtod('-42') == -42");
}

static void test_snprintf(void) {
    char buf[64];

    TEST_SUITE("libc_shim: snprintf");

    k_snprintf(buf, sizeof(buf), "hello %s", "world");
    ASSERT(strcmp(buf, "hello world") == 0, "%%s format");

    k_snprintf(buf, sizeof(buf), "n=%d", 42);
    ASSERT(strcmp(buf, "n=42") == 0, "%%d format");

    k_snprintf(buf, sizeof(buf), "n=%d", -7);
    ASSERT(strcmp(buf, "n=-7") == 0, "%%d negative");

    k_snprintf(buf, sizeof(buf), "c=%c", 'X');
    ASSERT(strcmp(buf, "c=X") == 0, "%%c format");

    k_snprintf(buf, sizeof(buf), "pct=%%");
    ASSERT(strcmp(buf, "pct=%") == 0, "%%%% literal");

    /* Test truncation */
    k_snprintf(buf, 5, "abcdefgh");
    ASSERT_EQ(buf[4], '\0', "truncation adds NUL");
    ASSERT(memcmp(buf, "abcd", 4) == 0, "truncation keeps prefix");
}

int main(void) {
    test_abs();
    test_memcpy();
    test_memset();
    test_memcmp();
    test_strlen();
    test_strcpy();
    test_strcmp();
    test_strncmp();
    test_strchr();
    test_strpbrk();
    test_strspn();
    test_floor();
    test_fmod();
    test_pow();
    test_strtod();
    test_snprintf();
    TEST_RESULTS();
}
