/*
 * MPF platform helpers (0x80089AE0-0x80089E50): mutex setup, memory and string
 * functions.
 */
#include "mpfmem.h"

/* 0x80089AE0: a mutex is a one-message queue holding its "key" */
void mpMutex_Init(MPF_Mutex *mutex) {
    osCreateMesgQueue(&mutex->mq, &mutex->msg, 1);
    mutex->owner = 0;
    mutex->count = 0;
    osSendMesg(&mutex->mq, NULL, OS_MESG_NOBLOCK);
}

/* 0x80089B30: overlap-safe copy; bcopy when the areas don't overlap */
void mpMemMove(void *dst, const void *src, u32 len) {
    u8 *d = dst;
    const u8 *s = src;
    s32 i;
    u32 u;

    if (s < d) {
        if (len < (u32)(d - s)) {
            bcopy(s, d, len);
        } else {
            for (i = len - 1; i >= 0; i--) {
                d[i] = s[i];
            }
        }
    } else {
        if (len < (u32)(s - d)) {
            bcopy(s, d, len);
        } else {
            for (u = 0; u < len; u++) {
                d[u] = s[u];
            }
        }
    }
}

/* 0x80089BD4 */
void mpMemSet(void *dst, u32 len, s32 value) {
    u32 i;

    if ((u8)value == 0) {
        bzero(dst, len);
    } else {
        for (i = 0; i < len; i++) {
            ((u8 *)dst)[i] = value;
        }
    }
}

/* 0x80089C20 */
void mpStrCpy(char *dst, const char *src) {
    while ((*dst++ = *src++) != 0) {
    }
}

/* 0x80089C3C */
void mpStrCat(char *dst, const char *src) {
    while (*dst++ != 0) {
    }
    dst--;
    while ((*dst++ = *src++) != 0) {
    }
}

/* 0x80089C68 */
s32 mpStrLen(const char *s) {
    const char *p = s;

    if (*p != 0) {
        p++;
        while (*p++ != 0) {
        }
    } else {
        p++;
    }
    return p - s - 1;
}

/* 0x80089C8C: -1 / 0 / 1 like strcmp */
s32 mpStrCmp(const char *a, const char *b) {
    for (;;) {
        u8 ca = *a++;
        u8 cb = *b++;

        if (ca == 0) {
            return -(cb != 0);
        }
        if (cb == 0) {
            return 1;
        }
        if (ca < cb) {
            return -1;
        }
        if (cb < ca) {
            return 1;
        }
    }
}

/*
 * The "no case" functions only fold characters from 'Z' (0x5A) upwards: the test is
 * c >= 'A' && c >= 'Z' (surely meant c <= 'Z'), and 0x20 is added. Folding never makes
 * two different letters equal, so in practice these compare case-sensitively:
 * "ABC" and "abc" are different strings to the game.
 */
#define BROKEN_FOLD(c) (((c) >= 'A' && (c) >= 'Z') ? (u8)((c) + 0x20) : (c))

/* 0x80089CDC */
s32 mpStrCmpNoCase(const char *a, const char *b) {
    for (;;) {
        u8 ca = *a++;
        u8 cb = *b++;

        if (ca == 0) {
            return -(cb != 0);
        }
        if (cb == 0) {
            return 1;
        }
        ca = BROKEN_FOLD(ca);
        cb = BROKEN_FOLD(cb);
        if (ca < cb) {
            return -1;
        }
        if (cb < ca) {
            return 1;
        }
    }
}

/* 0x80089D70: 1 if equal */
s32 mpStrEqual(const char *a, const char *b) {
    for (;;) {
        u8 ca = *a++;
        u8 cb = *b++;

        if (ca == 0) {
            return cb == 0;
        }
        if (cb == 0 || ca != cb) {
            return 0;
        }
    }
}

/* 0x80089DBC: 1 if equal (with the broken case folding above) */
s32 mpStrEqualNoCase(const char *a, const char *b) {
    for (;;) {
        u8 ca = *a++;
        u8 cb = *b++;

        if (ca == 0) {
            return cb == 0;
        }
        if (cb == 0) {
            return 0;
        }
        ca = BROKEN_FOLD(ca);
        cb = BROKEN_FOLD(cb);
        if (ca != cb) {
            return 0;
        }
    }
}
