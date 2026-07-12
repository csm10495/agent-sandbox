#include "string.h"

#ifndef UNIT_TEST
/* In freestanding mode we provide our own memset/memcpy/strlen etc. */

void *memset(void *dst, int c, size_t n) {
    uint8_t *p = dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t       *d = dst;
    const uint8_t *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (!n) return 0;
    return (uint8_t)*a - (uint8_t)*b;
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = '\0';
    return ret;
}

char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (*dst) dst++;
    while (n-- && (*dst = *src++)) dst++;
    *dst = '\0';
    return ret;
}

char *strchr(const char *s, int c) {
    for (; *s; s++)
        if (*s == (char)c) return (char *)s;
    return (c == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    for (; *s; s++)
        if (*s == (char)c) last = s;
    return (char *)last;
}

#endif /* !UNIT_TEST */

/* atoi, itoa, uitoa are available in both kernel and unit test modes */
int32_t atoi(const char *s) {
    int32_t result = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return sign * result;
}

/* Converts integer to string; returns buf */
char *itoa(int64_t n, char *buf, int base) {
    if (base < 2 || base > 36) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    char tmp[66]; int i = 0;
    bool neg = (base == 10 && n < 0);
    uint64_t v = neg ? (uint64_t)(-n) : (uint64_t)n;
    if (v == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    while (v) {
        int r = v % base;
        tmp[i++] = r < 10 ? '0' + r : 'a' + r - 10;
        v /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

char *uitoa(uint64_t n, char *buf, int base) {
    if (base < 2 || base > 36) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    char tmp[66]; int i = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    while (n) {
        int r = n % base;
        tmp[i++] = r < 10 ? '0' + r : 'a' + r - 10;
        n /= base;
    }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}
