/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef STRING
#define STRING

static void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

static int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

static size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return 0;
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *strcpy(char *dest, const char *src)
{
    char *saved = dest;
    while ((*dest++ = *src++))
        ;
    return saved;
}

static char* strcat(char* dest, const char* src) {
    strcpy(dest+strlen(dest),src);
    return dest;
}
static size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
}

static char *strncat(char *dest, const char *src, size_t n)
{
    char *ptr = dest;

    while (*ptr != '\0') {
        ptr++;
    }

    while (n > 0 && *src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
        n--;
    }

    *ptr = '\0';

    return dest;
}

static bool is_delimiter(char c, const char *delim) {
    while (*delim != '\0') {
        if (c == *delim) {
            return true;
        }
        delim++;
    }
    return false;
}

static char *strtok(char *str, const char *delim) {
    static char *next_token = NULL;

    if (str != NULL) {
        next_token = str;
    }

    if (next_token == NULL) {
        return NULL;
    }

    while (*next_token != '\0' && is_delimiter(*next_token, delim)) {
        next_token++;
    }

    if (*next_token == '\0') {
        next_token = NULL;
        return NULL;
    }

    char *token_start = next_token;

    while (*next_token != '\0' && !is_delimiter(*next_token, delim)) {
        next_token++;
    }

    if (*next_token != '\0') {
        *next_token = '\0';
        next_token++;
    } else {
        next_token = NULL;
    }

    return token_start;
}

static inline bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static inline int char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

static long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc = 0;
    bool neg = false;
    bool any = false;
    bool overflow = false;

    if (base < 0 || base == 1 || base > 36) {
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }

    while (is_space(*s)) {
        s++;
    }

    if (*s == '-') {
        neg = true;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        char_to_val(s[2]);
        if (char_to_val(s[2]) >= 0 && char_to_val(s[2]) < 16) {
            s += 2;
            base = 16;
        }
    }

    if (base == 0) {
        if (*s == '0') {
            base = 8;
        } else {
            base = 10;
        }
    }

    unsigned long limit = neg ? ((unsigned long)-(LONG_MIN + 1) + 1) : LONG_MAX;
    unsigned long cutoff = limit / base;
    int cutlim = limit % base;

    int val;
    while ((val = char_to_val(*s)) >= 0 && val < base) {
        any = true;
        if (overflow) {
            s++;
            continue;
        }

        if (acc > cutoff || (acc == cutoff && val > cutlim)) {
            overflow = true;
        } else {
            acc = acc * base + val;
        }
        s++;
    }

    if (endptr) {
        *endptr = (char *)(any ? s : nptr);
    }

    if (overflow) {
        return neg ? LONG_MIN : LONG_MAX;
    }

    return neg ? -(long)acc : (long)acc;
}


#endif
