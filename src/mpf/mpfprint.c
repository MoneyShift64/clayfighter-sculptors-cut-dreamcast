/*
 * Interplay's printf (0x80089E50-0x8008B050), used for every error message.
 *
 * Supports %c %s %d %i %u %x %X %b (binary) %f %n %p %P %%, the flags - + space # 0,
 * a width and precision (numbers or *), and the size letters h l L N. Quirks kept
 * from the original:
 *   - %c prints the top byte of the argument word, and %h.. numbers use the top
 *     half (on the big-endian N64 the character / short sits in the bottom part, so
 *     these print the wrong thing)
 *   - %f goes through libultra's sprintf after a round trip through single precision
 *   - %e %E %g %G print nothing and don't use up an argument
 *   - %L numbers keep only the low 16 bits; precision only affects %s
 *   - the output isn't limited in length
 */
#include "mpfplat.h"

void sprintf(char *buf, const char *fmt, ...);   /* libultra (still in assembly) */

typedef struct FmtSpec {
    /* 0x00 */ s32 alt;        /* '#': accepted but unused */
    /* 0x04 */ s32 plus;       /* '+' */
    /* 0x08 */ s32 space;      /* ' ' */
    /* 0x0C */ s32 left;       /* '-' */
    /* 0x10 */ s32 width;      /* -1 = none */
    /* 0x14 */ s32 zero;       /* '0' */
    /* 0x18 */ s32 precision;  /* -1 = none */
    /* 0x1C */ s32 size;       /* 16 ('h'), 32, 64 ('L') */
} FmtSpec;

char *mpFormatDecimal(char *out, u32 value, s32 isSigned, FmtSpec *spec);
char *mpFormatHex(char *out, u32 value, const char *digits, FmtSpec *spec);
char *mpFormatBinary(char *out, u32 value, FmtSpec *spec);

static const char sLower[] = "0123456789abcdef";
static const char sUpper[] = "0123456789ABCDEF";

/* padding, digits, padding - shared by the number formatters */
static char *EmitNumber(char *out, const char *digits, s32 n, FmtSpec *spec) {
    s32 i;

    if (spec->width >= 0 && !spec->left) {
        char pad = spec->zero ? '0' : ' ';
        for (i = n; i < spec->width; i++) {
            *out++ = pad;
        }
    }
    for (i = 0; i < n; i++) {
        *out++ = digits[i];
    }
    if (spec->width > 0 && spec->left) {
        for (i = n; i < spec->width; i++) {
            *out++ = ' ';
        }
    }
    return out;
}

/* 0x8008A44C */
char *mpFormatDecimal(char *out, u32 value, s32 isSigned, FmtSpec *spec) {
    char digits[16];
    s32 n = 0;
    char sign = 0;
    u32 p, q;
    s32 i;

    if (spec->size == 16) {
        if (isSigned && (value & 0x8000)) {
            value |= 0xFFFF0000;
        } else {
            value &= 0xFFFF;
        }
    }
    if (isSigned) {
        if ((s32)value < 0) {
            sign = '-';
            spec->width--;
            value = -value;
        } else if (spec->plus) {
            sign = '+';
            spec->width--;
        } else if (spec->space) {
            sign = ' ';
            spec->width--;
        }
    }
    if (spec->size == 32) {
        p = 1000000000;
    } else {
        value &= 0xFFFF;
        p = 10000;
    }
    for (; p >= 10; p /= 10) {
        q = value / p;
        if (n != 0 || q != 0) {
            digits[n++] = '0' + q;
        }
        value -= q * p;
    }
    digits[n++] = '0' + value;

    /* the sign goes before zero padding but after space padding */
    if (spec->zero && sign) {
        *out++ = sign;
    }
    if (spec->width >= 0 && !spec->left) {
        char pad = spec->zero ? '0' : ' ';
        for (i = n; i < spec->width; i++) {
            *out++ = pad;
        }
    }
    if (!spec->zero && sign) {
        *out++ = sign;
    }
    for (i = 0; i < n; i++) {
        *out++ = digits[i];
    }
    if (spec->width > 0 && spec->left) {
        for (i = n; i < spec->width; i++) {
            *out++ = ' ';
        }
    }
    return out;
}

/* 0x8008A900: 8 digits for 32-bit values, 4 otherwise; no "0x" even with '#' */
char *mpFormatHex(char *out, u32 value, const char *digits, FmtSpec *spec) {
    char buf[8];
    s32 n = 0;
    s32 shift;

    for (shift = (spec->size == 32) ? 28 : 12; shift > 0; shift -= 4) {
        u32 d = (value >> shift) & 0xF;
        if (d != 0 || n != 0) {
            buf[n++] = digits[d];
        }
    }
    buf[n++] = digits[value & 0xF];
    return EmitNumber(out, buf, n, spec);
}

/* 0x8008AB14 */
char *mpFormatBinary(char *out, u32 value, FmtSpec *spec) {
    char buf[32];
    s32 n = 0;
    s32 bit;

    for (bit = (spec->size == 32) ? 31 : 15; bit > 0; bit--) {
        if ((value >> bit) & 1) {
            buf[n++] = '1';
        } else if (n != 0) {
            buf[n++] = '0';
        }
    }
    buf[n++] = (value & 1) ? '1' : '0';
    return EmitNumber(out, buf, n, spec);
}

/* 0x80089E50 */
void mpSPrintf(char *buf, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    mpVSPrintf(buf, fmt, args);
    va_end(args);
}

/* reads an integer argument as the original does for the current size */
#define INT_ARG(args, spec) ((spec).size == 16 ? (u32)(u16)(va_arg(args, u32) >> 16) : va_arg(args, u32))

/* 0x80089E78 */
void mpVSPrintf(char *buf, const char *fmt, va_list args) {
    char *out = buf;
    FmtSpec spec;
    char tmp[0x6C];
    u8 c;
    s32 i, n;

    c = *fmt++;
    while (c != 0) {
        if (c != '%') {
            *out++ = c;
            c = *fmt++;
            continue;
        }
        spec.alt = 0;
        spec.plus = 0;
        spec.space = 0;
        spec.left = 0;
        spec.width = -1;
        spec.zero = 0;
        spec.precision = -1;
        spec.size = 32;
        for (;;) {
            c = *fmt++;
            if (c == 0) {
                goto done;
            }
            if (c == 'c') {
                *out++ = (u8)(va_arg(args, u32) >> 24);
                break;
            }
            if (c == 's') {
                const char *s = va_arg(args, const char *);
                if (spec.precision == -1) {
                    while ((*out++ = *s++) != 0) {
                    }
                    out--;
                } else if (spec.precision > 0) {
                    for (i = 0; i < spec.precision; i++) {
                        char ch = *s++;
                        *out++ = ch;
                        if (ch == 0) {
                            out--;
                            break;
                        }
                    }
                }
                break;
            }
            if (c == '%') {
                *out++ = '%';
                break;
            }
            if (c == 'd' || c == 'i' || c == 'u') {
                u32 v = INT_ARG(args, spec);
                out = mpFormatDecimal(out, v, c != 'u', &spec);
                break;
            }
            if (c == 'x' || c == 'X') {
                u32 v = INT_ARG(args, spec);
                out = mpFormatHex(out, v, c == 'x' ? sLower : sUpper, &spec);
                break;
            }
            if (c == 'b') {
                u32 v = INT_ARG(args, spec);
                out = mpFormatBinary(out, v, &spec);
                break;
            }
            if (c == 'f') {
                f32 f = (f32)va_arg(args, double);
                sprintf(tmp, spec.plus ? "%+f" : "%f", (double)f);
                for (i = 0; tmp[i] != 0; i++) {
                    *out++ = tmp[i];
                }
                break;
            }
            if (c == 'e' || c == 'E' || c == 'g' || c == 'G') {
                break;
            }
            if (c == 'n') {
                s32 *p = va_arg(args, s32 *);
                *p = out - buf;
                break;
            }
            if (c == 'p' || c == 'P') {
                spec.size = 32;
                spec.width = 8;
                spec.zero = 1;
                out = mpFormatHex(out, va_arg(args, u32), c == 'p' ? sLower : sUpper, &spec);
                break;
            }
            if (c == 'l' || c == 'N') {
                spec.size = 32;
                continue;
            }
            if (c == 'L') {
                spec.size = 64;
                continue;
            }
            if (c == 'h') {
                spec.size = 16;
                continue;
            }
            if (c == '0') {
                spec.zero = 1;
                continue;
            }
            if (c == '*') {
                spec.width = va_arg(args, s32);
                continue;
            }
            if ((u8)(c - '0') < 10) {
                n = c - '0';
                for (;;) {
                    c = *fmt++;
                    if (c == 0) {
                        goto done;
                    }
                    if ((u8)(c - '0') >= 10) {
                        break;
                    }
                    n = n * 10 + c - '0';
                }
                fmt--;
                spec.width = n;
                continue;
            }
            if (c == '.') {
                c = *fmt++;
                if (c == 0) {
                    goto done;
                }
                if (c == '*') {
                    spec.precision = va_arg(args, s32);
                    continue;
                }
                if ((u8)(c - '0') >= 10) {
                    break;          /* gives up on this conversion */
                }
                n = c - '0';
                for (;;) {
                    c = *fmt++;
                    if (c == 0) {
                        goto done;
                    }
                    if ((u8)(c - '0') >= 10) {
                        break;
                    }
                    n = n * 10 + c - '0';
                }
                fmt--;
                spec.precision = n;
                continue;
            }
            if (c == '-') {
                spec.left = 1;
                continue;
            }
            if (c == '+') {
                spec.plus = 1;
                continue;
            }
            if (c == ' ') {
                spec.space = 1;
                continue;
            }
            if (c == '#') {
                spec.alt = 1;
                continue;
            }
            break;                  /* unknown letter: dropped */
        }
        c = *fmt++;
    }
done:
    *out = 0;
}

/* 0x8008B030 */
void mpNullFunc(void) {
}

/* 0x8008B038: debug output, empty in this build */
void mpDebugPrintf(const char *fmt, ...) {
    (void)fmt;
}
