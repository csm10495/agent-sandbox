/* tests/unit/framework.h - Minimal test framework */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int test_pass, test_fail;

#define ASSERT_EQ(a, b) do { \
    if ((a) == (b)) { test_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " #a " != " #b "\n", __FILE__, __LINE__); test_fail++; } \
} while(0)

#define ASSERT_NE(a, b) do { \
    if ((a) != (b)) { test_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " #a " == " #b "\n", __FILE__, __LINE__); test_fail++; } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    if (strcmp((a),(b)) == 0) { test_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a),(b)); test_fail++; } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (expr) { test_pass++; } \
    else { fprintf(stderr, "FAIL %s:%d: " #expr " is false\n", __FILE__, __LINE__); test_fail++; } \
} while(0)

#define ASSERT_NULL(p)  ASSERT_EQ((void*)(p), (void*)NULL)
#define ASSERT_NONNULL(p) ASSERT_NE((void*)(p), (void*)NULL)

#define RUN_SUITE(name) do { \
    extern void suite_##name(void); \
    printf("  %-20s", #name); fflush(stdout); \
    int before_fail = test_fail; \
    suite_##name(); \
    printf("%s\n", (test_fail == before_fail) ? "OK" : "FAIL"); \
} while(0)
