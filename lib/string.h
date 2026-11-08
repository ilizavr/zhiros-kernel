/*
 * Copyright (c) 2026 ilizavr & yellowhat
 * SPDX-License-Identifier: MIT
 */

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef STRING
#define STRING

static char *strstr(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL)
        return NULL;
    if (*needle == '\0')
        return (char *)haystack;

    for (; *haystack != '\0'; ++haystack) {
        const char *h = haystack;
        const char *n = needle;

        while (*h != '\0' && *n != '\0' && *h == *n) {
            ++h;
            ++n;
        }

        if (*n == '\0')
            return (char *)haystack;
    }

    return NULL;
}
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
static void* memcpy_wc(void* dest, void* src, size_t n) {
    u8* d = (u8*)dest;
    u8* s = (u8*)src;

    size_t blocks = n / 64;

    if (blocks > 0) {
        asm volatile (
            "1:\n\t"
            "movdqu   0(%1), %%xmm0\n\t"
            "movdqu  16(%1), %%xmm1\n\t"
            "movdqu  32(%1), %%xmm2\n\t"
            "movdqu  48(%1), %%xmm3\n\t"

            "movntdq %%xmm0,   0(%0)\n\t"
            "movntdq %%xmm1,  16(%0)\n\t"
            "movntdq %%xmm2,  32(%0)\n\t"
            "movntdq %%xmm3,  48(%0)\n\t"

            "add $64, %1\n\t"
            "add $64, %0\n\t"
            "dec %2\n\t"
            "jnz 1b\n\t"

            "sfence\n\t"
            : "+r" (d), "+r" (s), "+r" (blocks)
            :
            : "memory", "xmm0", "xmm1", "xmm2", "xmm3"
        );
    }

    size_t tail = n % 64;
    if (tail > 0) {
        asm volatile (
            "rep movsb"
            : "+D" (d), "+S" (s), "+c" (tail)
            :
            : "memory"
        );
    }

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


static const u16 crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0,
};

static u16 crc16(const u8* data, u16 len)
{
    u16 crc = 0xFFFF;
    while (len--)
        crc = (u16)((crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF]);
    return crc;
}

#endif
