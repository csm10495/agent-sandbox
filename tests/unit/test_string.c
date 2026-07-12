/* tests/unit/test_string.c */
#include "framework.h"
#include "string.h"

void suite_string(void) {
    /* strlen */
    ASSERT_EQ(strlen(""),      0u);
    ASSERT_EQ(strlen("abc"),   3u);
    ASSERT_EQ(strlen("hello"), 5u);

    /* strcmp */
    ASSERT_EQ(strcmp("abc", "abc"),  0);
    ASSERT_TRUE(strcmp("abc", "abd") < 0);
    ASSERT_TRUE(strcmp("b",   "a")   > 0);
    ASSERT_EQ(strcmp("", ""),         0);

    /* strncmp */
    ASSERT_EQ(strncmp("abcX", "abcY", 3), 0);
    ASSERT_TRUE(strncmp("abc", "abd", 4)  < 0);

    /* strcpy */
    char buf[32];
    strcpy(buf, "hello");
    ASSERT_STREQ(buf, "hello");

    /* strcat */
    strcpy(buf, "foo");
    strcat(buf, "bar");
    ASSERT_STREQ(buf, "foobar");

    /* strchr */
    ASSERT_NE(strchr("hello", 'l'), (char*)NULL);
    ASSERT_NULL(strchr("hello", 'z'));

    /* memset / memcpy / memcmp */
    uint8_t a[8], b[8];
    memset(a, 0xAB, 8);
    for (int i = 0; i < 8; i++) ASSERT_EQ(a[i], 0xAB);
    memcpy(b, a, 8);
    ASSERT_EQ(memcmp(a, b, 8), 0);
    b[3] = 0;
    ASSERT_TRUE(memcmp(a, b, 8) != 0);

    /* itoa */
    char tmp[32];
    itoa(42,   tmp, 10); ASSERT_STREQ(tmp, "42");
    itoa(-99,  tmp, 10); ASSERT_STREQ(tmp, "-99");
    itoa(255,  tmp, 16); ASSERT_STREQ(tmp, "ff");
    uitoa(255u, tmp, 16); ASSERT_STREQ(tmp, "ff");

    /* atoi */
    ASSERT_EQ(atoi("0"),    0);
    ASSERT_EQ(atoi("42"),  42);
    ASSERT_EQ(atoi("-7"),  -7);
    ASSERT_EQ(atoi("  5"),  5);
}
