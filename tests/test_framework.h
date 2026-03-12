#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char* g_current_suite = "";

#define TEST_SUITE(name) \
    do { g_current_suite = name; printf("\n=== %s ===\n", name); } while(0)

#define ASSERT(cond, msg) do { \
    if (cond) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        g_tests_failed++; \
        printf("  [FAIL] %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
    } \
} while(0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_NEQ(a, b, msg) ASSERT((a) != (b), msg)
#define ASSERT_NULL(a, msg) ASSERT((a) == NULL, msg)
#define ASSERT_NOT_NULL(a, msg) ASSERT((a) != NULL, msg)
#define ASSERT_STR_EQ(a, b, msg) ASSERT(strcmp((a), (b)) == 0, msg)

#define TEST_RESULTS() do { \
    printf("\n--- Results ---\n"); \
    printf("Passed: %d\n", g_tests_passed); \
    printf("Failed: %d\n", g_tests_failed); \
    return g_tests_failed > 0 ? 1 : 0; \
} while(0)

#endif
