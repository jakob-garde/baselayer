#ifndef __BASE_H__
#define __BASE_H__


#pragma warning(push)
#pragma warning(disable : 4200)
#pragma warning(disable : 4477)
#pragma warning(disable : 4996)
#pragma warning(disable : 4530)


#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>


//
// basics


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;


#ifdef __arm__
#warning "WARN: __arm__ detected: u64 typedef'd to 32bit"
typedef uint32_t u64;
#else
typedef uint64_t u64;
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

#define KILOBYTE 1024
#define SIXTEEN_KB (16 * 1024)
#define THIRTYTWO_KB (32 * 1024)
#define SIXTYFOUR_KB (64 * 1024)
#define MEGABYTE (1024 * 1024)
#define SIXTEEN_MB (16 * 1024 * 1024)
#define GIGABYTE (1024 * 1024 * 1024)
#define FOUR_GB (4 * 1024 * 1024 * 1024)


#define PI 3.14159f
f32 deg2rad = PI / 180.0f;
f32 rad2deg = 180.0f / PI;


inline u8 MinU8(u8 a, u8 b) { return (a <= b) ? a : b; }
inline u16 MinU16(u16 a, u16 b) { return (a <= b) ? a : b; }
inline u32 MinU32(u32 a, u32 b) { return (a <= b) ? a : b; }
inline u64 MinU64(u64 a, u64 b) { return (a <= b) ? a : b; }

inline s8 MinS8(s8 a, s8 b) { return (a <= b) ? a : b; }
inline s16 MinS16(s16 a, s16 b) { return (a <= b) ? a : b; }
inline s32 MinS32(s32 a, s32 b) { return (a <= b) ? a : b; }
inline s64 MinS64(s64 a, s64 b) { return (a <= b) ? a : b; }

inline f32 MinF32(f32 a, f32 b) { return (a <= b) ? a : b; }
inline f64 MinF64(f64 a, f64 b) { return (a <= b) ? a : b; }

inline u8 MaxU8(u8 a, u8 b) { return (a > b) ? a : b; }
inline u16 MaxU16(u16 a, u16 b) { return (a > b) ? a : b; }
inline u32 MaxU32(u32 a, u32 b) { return (a > b) ? a : b; }
inline u64 MaxU64(u64 a, u64 b) { return (a > b) ? a : b; }

inline s8 MaxS8(s8 a, s8 b) { return (a > b) ? a : b; }
inline s16 MaxS16(s16 a, s16 b) { return (a > b) ? a : b; }
inline s32 MaxS32(s32 a, s32 b) { return (a > b) ? a : b; }
inline s64 MaxS64(s64 a, s64 b) { return (a > b) ? a : b; }

inline f32 MaxF32(f32 a, f32 b) { return (a > b) ? a : b; }
inline f64 MaxF64(f64 a, f64 b) { return (a > b) ? a : b; }


inline void _memzero(void *dest, size_t n) {
    memset(dest, 0, n);
}


//
// linked list


struct LList1 {
    LList1 *next;
};

struct LList2 {
    LList2 *next;
    LList2 *prev;
};

struct LList3 {
    LList3 *next;
    LList3 *prev;
    LList3 *descend;
};

void *InsertBefore1(void *newlink, void *before) {
    LList1 *newlnk = (LList1*) newlink;

    newlnk->next = (LList1*) before;
    return newlink;
}
void *InsertAfter1(void *after, void *newlink) {
    ((LList1*) after)->next = (LList1*) newlink;
    return newlink;
}
void InsertBefore2(void *newlink, void *before) {
    LList2 *newlnk = (LList2*) newlink;
    LList2 *befre = (LList2*) before;

    newlnk->prev = befre->prev;
    newlnk->next = befre;
    if (befre->prev != NULL) {
        befre->prev->next = newlnk;
    }
    befre->prev = newlnk;
}
void InsertBelow3(void *newlink, void *below) {
    LList3 *newlnk = (LList3*) newlink;
    LList3 *belw = (LList3*) below;

    newlnk->descend = belw->descend;
    belw->descend = newlnk;
}


//
// string conversion


u32 ParseInt(char *text) {
    u32 val = 0;
    u32 multiplier = 1;

    // signed?
    bool sgned = text[0] == '-';
    if (sgned) {
        ++text;
    }
    u32 len = (u32) strlen(text);

    // decimals before dot
    for (u32 i = 0; i < len; ++i) {
        val += (text[len - 1 - i] - 48) * multiplier;
        multiplier *= 10;
    }

    // handle the sign
    if (sgned) {
        val *= -1;
    }

    return val;
}

u32 ParseInt(char *text, u32 len) {
    u32 val = 0;
    u32 multiplier = 1;

    // signed?
    bool sgned = text[0] == '-';
    if (sgned) {
        ++text;
    }

    // decimals before dot
    for (u32 i = 0; i < len; ++i) {
        val += (text[len - 1 - i] - 48) * multiplier;
        multiplier *= 10;
    }

    // handle the sign
    if (sgned) {
        val *= -1;
    }

    return val;
}

f64 ParseDouble(char *str, u8 len) {
    f64 val = 0;
    f64 multiplier = 1;

    // handle sign
    bool sgned = str[0] == '-';
    if (sgned) {
        ++str;
        --len;
    }
    if (len == 0) {
        return 0.0;
    }

    u8 decs_denom = 0;
    while ((str[decs_denom] != '.') && (decs_denom < len)) {
        ++decs_denom;
    }

    // decimals before dot
    for (int i = 0; i < decs_denom; ++i) {
        char ascii = str[decs_denom - 1 - i];
        u8 decimal = ascii - 48;
        val += decimal * multiplier;
        multiplier *= 10;
    }

    // decimals after dot
    multiplier = 0.1f;
    u8 decs_nom = len - 1 - decs_denom;
    for (int i = decs_denom + 1; i < len; ++i) {
        char ascii = str[i];
        u8 decimal = ascii - 48;
        val += decimal * multiplier;
        multiplier *= 0.1;
    }

    // handle the sign
    if (sgned) {
        val *= -1;
    }

    return val;
}


//
// cmd-line args


s32 g_argc;
char **g_argv;
void CLAInit(s32 argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

bool CLAContainsArg(const char *search, int argc, char **argv, int *idx = NULL) {
    for (int i = 0; i < argc; ++i) {
        char *arg = argv[i];
        if (!strcmp(argv[i], search)) {
            if (idx != NULL) {
                *idx = i;
            }
            return true;
        }
    }
    return false;
}

bool CLAContainsArgs(const char *search_a, const char *search_b, int argc, char **argv) {
    bool found_a = false;
    bool found_b = false;
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], search_a)) {
            found_a = true;
        }
        if (!strcmp(argv[i], search_b)) {
            found_b = true;
        }
    }
    return found_a && found_b;
}

char *CLAGetArgValue(const char *key, int argc, char **argv) {
    int i;
    bool error = !CLAContainsArg(key, argc, argv, &i) || i == argc - 1;;
    if (error == false) {
        char *val = argv[i+1];
        error = strlen(val) > 1 && val[0] == '-' && val[1] == '-';
    }
    if (error == true) {
        printf("KW arg %s must be followed by a value arg\n", key);
        return NULL;
    }
    return argv[i+1];
}

char *CLAGetFirstArg(int argc, char **argv) {
    if (argc <= 1) {
        return NULL;
    }
    return argv[1];
}

#endif
