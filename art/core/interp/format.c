// ============================================================
// format.c — the `${expr:spec}` mini-language
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "format.h"
#include "interp.h"
#include "gc.h"

// Parsed form of a format spec. All fields optional; a spec of
// "" produces no padding and the default type for the value.
typedef struct
{
    uint16_t fill; // fill character, default ' '
    char align;    // '<' left, '>' right, '^' center, 0 = default
    char sign;     // '+' always, ' ' for space, '-' only negative (default)
    bool alt;      // '#' alternate form
    bool zero_pad; // '0' flag
    int width;     // -1 if absent
    int precision; // -1 if absent
    char type;     // 0 if absent, else one of d i x X o f e g s c
} FormatSpec;

static void spec_defaults(FormatSpec *fs)
{
    fs->fill = ' ';
    fs->align = 0;
    fs->sign = 0;
    fs->alt = false;
    fs->zero_pad = false;
    fs->width = -1;
    fs->precision = -1;
    fs->type = 0;
}

static bool is_align_char(uint16_t c)
{
    return c == '<' || c == '>' || c == '^';
}

// Parse a spec. On any malformed input, raises a runtime error
// with a message naming the problem. Never returns partially
// initialized state — the caller checks the ArtState error flag.
static void parse_spec(ArtState *S, ObjString *spec, FormatSpec *out)
{
    spec_defaults(out);

    int i = 0;
    int n = spec->unit_count;

    // [fill]align
    //
    // The fill character is only recognized when it directly
    // precedes an align character. That's what lets `05d` mean
    // "zero-pad width 5" instead of "fill='0', align='5d'".
    if (n >= 2 && is_align_char(spec->chars[1]))
    {
        out->fill = spec->chars[0];
        out->align = (char)spec->chars[1];
        i = 2;
    }
    else if (n >= 1 && is_align_char(spec->chars[0]))
    {
        out->align = (char)spec->chars[0];
        i = 1;
    }

    // sign
    if (i < n && (spec->chars[i] == '+' ||
                  spec->chars[i] == '-' ||
                  spec->chars[i] == ' '))
    {
        out->sign = (char)spec->chars[i];
        i++;
    }

    // #
    if (i < n && spec->chars[i] == '#')
    {
        out->alt = true;
        i++;
    }

    // 0
    if (i < n && spec->chars[i] == '0')
    {
        out->zero_pad = true;
        i++;
    }

    // width
    if (i < n && spec->chars[i] >= '0' && spec->chars[i] <= '9')
    {
        int w = 0;
        while (i < n && spec->chars[i] >= '0' && spec->chars[i] <= '9')
        {
            w = w * 10 + (int)(spec->chars[i] - '0');
            i++;
            if (w > 1000000)
                w = 1000000;
        }
        out->width = w;
    }

    // .precision
    if (i < n && spec->chars[i] == '.')
    {
        i++;
        int p = 0;
        bool any = false;
        while (i < n && spec->chars[i] >= '0' && spec->chars[i] <= '9')
        {
            p = p * 10 + (int)(spec->chars[i] - '0');
            i++;
            any = true;
            if (p > 100000)
                p = 100000;
        }
        if (!any)
            art_runtime_error(S, NULL,
                              "format spec: expected digits after '.'");
        out->precision = p;
    }

    // type
    if (i < n)
    {
        out->type = (char)spec->chars[i];
        i++;
    }

    if (i != n)
        art_runtime_error(S, NULL,
                          "format spec: unexpected trailing characters");

    // Validate the type if present.
    if (out->type != 0)
    {
        switch (out->type)
        {
        case 'd':
        case 'i':
        case 'x':
        case 'X':
        case 'o':
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 's':
        case 'c':
            break;
        default:
            art_runtime_error(S, NULL,
                              "format spec: unknown type '%c'", out->type);
        }
    }
}

// ============================================================
// Growable UTF-16 buffer
// ============================================================

typedef struct
{
    uint16_t *buf;
    int len;
    int cap;
} FmtBuf;

static void fb_init(FmtBuf *fb, int initial)
{
    fb->cap = initial < 32 ? 32 : initial;
    fb->buf = malloc(sizeof(uint16_t) * fb->cap);
    fb->len = 0;
}

static void fb_ensure(FmtBuf *fb, int extra)
{
    if (fb->len + extra > fb->cap)
    {
        while (fb->len + extra > fb->cap)
            fb->cap *= 2;
        fb->buf = realloc(fb->buf, sizeof(uint16_t) * fb->cap);
    }
}

static void fb_append(FmtBuf *fb, const uint16_t *src, int n)
{
    if (n <= 0)
        return;
    fb_ensure(fb, n);
    memcpy(fb->buf + fb->len, src, sizeof(uint16_t) * n);
    fb->len += n;
}

static void fb_fill(FmtBuf *fb, uint16_t c, int n)
{
    if (n <= 0)
        return;
    fb_ensure(fb, n);
    for (int i = 0; i < n; i++)
        fb->buf[fb->len++] = c;
}

// Emit `s` (length `n`) with alignment and width from the spec.
// Strings default to left-aligned; everything else right-aligns.
static void emit_padded(FmtBuf *fb, const uint16_t *s, int n,
                        FormatSpec *fs, bool is_text)
{
    int pad = 0;
    if (fs->width > n)
        pad = fs->width - n;

    char align = fs->align;
    if (align == 0)
        align = is_text ? '<' : '>';

    int left_pad = 0, right_pad = 0;
    switch (align)
    {
    case '<':
        right_pad = pad;
        break;
    case '>':
        left_pad = pad;
        break;
    case '^':
        left_pad = pad / 2;
        right_pad = pad - left_pad;
        break;
    }

    fb_fill(fb, fs->fill, left_pad);
    fb_append(fb, s, n);
    fb_fill(fb, fs->fill, right_pad);
}

// ----- integers -----

static void format_int(FmtBuf *fb, int64_t v, FormatSpec *fs)
{
    char type = fs->type ? fs->type : 'd';

    // Build the digits into a scratch buffer first, then pad.
    char digits[64];
    int dn = 0;

    bool negative = v < 0;
    uint64_t uv = negative ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;

    if (type == 'd' || type == 'i')
    {
        if (uv == 0)
        {
            digits[dn++] = '0';
        }
        else
        {
            char tmp[32];
            int tn = 0;
            while (uv > 0)
            {
                tmp[tn++] = (char)('0' + (uv % 10));
                uv /= 10;
            }
            while (tn > 0)
                digits[dn++] = tmp[--tn];
        }
    }
    else if (type == 'x' || type == 'X' || type == 'o')
    {
        const char *base_digits = (type == 'X')
                                      ? "0123456789ABCDEF"
                                      : "0123456789abcdef";
        int base = (type == 'o') ? 8 : 16;

        if (uv == 0)
        {
            digits[dn++] = '0';
        }
        else
        {
            char tmp[32];
            int tn = 0;
            while (uv > 0)
            {
                tmp[tn++] = base_digits[uv % base];
                uv /= base;
            }
            while (tn > 0)
                digits[dn++] = tmp[--tn];
        }

        if (fs->alt)
        {
            // Prefix. 0x / 0X for hex, 0o for octal.
            if (type == 'o')
            {
                digits[dn++] = '\0'; // placeholder so we can shift
                // Simpler: rebuild with prefix at start.
                dn = 0;
                digits[dn++] = '0';
                digits[dn++] = 'o';
                char rev[32];
                int rn = 0;
                uint64_t x = (uint64_t)v;
                if (negative)
                    x = (uint64_t)(-v);
                if (x == 0)
                {
                    rev[rn++] = '0';
                }
                else
                {
                    while (x > 0)
                    {
                        rev[rn++] = base_digits[x % 8];
                        x /= 8;
                    }
                }
                while (rn > 0)
                    digits[dn++] = rev[--rn];
            }
            else
            {
                char rev[32];
                int rn = 0;
                uint64_t x = negative ? (uint64_t)(-v) : (uint64_t)v;
                if (x == 0)
                {
                    rev[rn++] = '0';
                }
                else
                {
                    while (x > 0)
                    {
                        rev[rn++] = base_digits[x % base];
                        x /= base;
                    }
                }
                dn = 0;
                digits[dn++] = '0';
                digits[dn++] = (type == 'X') ? 'X' : 'x';
                while (rn > 0)
                    digits[dn++] = rev[--rn];
            }
        }
    }

    // Assemble with sign.
    uint16_t out[96];
    int on = 0;

    // For alt-form hex/oct, the prefix is already in `digits`.
    bool prefix_in_digits = fs->alt &&
                            (type == 'x' || type == 'X' || type == 'o');

    if (!prefix_in_digits)
    {
        if (negative)
            out[on++] = '-';
        else if (fs->sign == '+')
            out[on++] = '+';
        else if (fs->sign == ' ')
            out[on++] = ' ';
    }

    for (int i = 0; i < dn; i++)
        out[on++] = (uint16_t)digits[i];

    // Zero-pad: insert zeros after the sign/prefix, before the
    // digits. Only when 0 flag is set and no explicit fill.
    if (fs->zero_pad && fs->width > on && fs->fill == ' ' && fs->align == 0)
    {
        int pad = fs->width - on;
        int insert_at = 0;
        // Skip over sign.
        if (out[0] == '-' || out[0] == '+' || out[0] == ' ')
            insert_at = 1;
        // Skip over 0x/0X/0o prefix.
        if (prefix_in_digits)
        {
            if (out[insert_at] == '0' &&
                (out[insert_at + 1] == 'x' ||
                 out[insert_at + 1] == 'X' ||
                 out[insert_at + 1] == 'o'))
                insert_at += 2;
        }

        // Shift the tail right to make room.
        for (int i = on - 1; i >= insert_at; i--)
            out[i + pad] = out[i];
        for (int i = 0; i < pad; i++)
            out[insert_at + i] = '0';
        on += pad;
    }

    emit_padded(fb, out, on, fs, false);
}

// ----- floats -----

static void format_float(FmtBuf *fb, double v, FormatSpec *fs)
{
    char type = fs->type ? fs->type : 'g';

    // Build a printf format string.
    char cfmt[32];
    int ci = 0;
    cfmt[ci++] = '%';

    if (fs->sign == '+')
        cfmt[ci++] = '+';
    else if (fs->sign == ' ')
        cfmt[ci++] = ' ';

    if (fs->alt)
        cfmt[ci++] = '#';

    if (fs->zero_pad && fs->fill == ' ' && fs->align == 0)
        cfmt[ci++] = '0';

    // Translate alignment to a leading '-' (left) or nothing.
    // Note: our padding layer handles fill/width; C's format
    // width would conflict. So we leave width out of the C
    // format and let emit_padded apply it.
    if (fs->precision >= 0)
    {
        ci += snprintf(cfmt + ci, sizeof(cfmt) - ci,
                       ".%d", fs->precision);
    }

    char tc = type;
    if (tc == 'F')
        tc = 'f';
    if (tc == 'E')
        tc = 'e';
    if (tc == 'G')
        tc = 'g';

    cfmt[ci++] = tc;
    cfmt[ci] = '\0';

    char buf[512];
    int n = snprintf(buf, sizeof(buf), cfmt, v);
    if (n < 0)
        n = 0;
    if (n >= (int)sizeof(buf))
        n = (int)sizeof(buf) - 1;

    // Copy bytes into a UTF-16 scratch, then pad.
    uint16_t scratch[512];
    for (int i = 0; i < n; i++)
        scratch[i] = (uint16_t)(unsigned char)buf[i];

    // Zero-pad before the sign if requested. Handled here rather
    // than through C's %0 because we're not passing a width.
    if (fs->zero_pad && fs->fill == ' ' && fs->align == 0 &&
        fs->width > n)
    {
        int pad = fs->width - n;
        int insert_at = 0;
        if (scratch[0] == '-' || scratch[0] == '+' || scratch[0] == ' ')
            insert_at = 1;

        for (int i = n - 1; i >= insert_at; i--)
            scratch[i + pad] = scratch[i];
        for (int i = 0; i < pad; i++)
            scratch[insert_at + i] = '0';
        n += pad;
    }

    emit_padded(fb, scratch, n, fs, false);
}

// ----- strings -----

static void format_string(ArtState *S, FmtBuf *fb, ObjString *s,
                          FormatSpec *fs)
{
    (void)S;
    int n = s->unit_count;

    // Precision on a string caps its length.
    if (fs->precision >= 0 && fs->precision < n)
        n = fs->precision;

    emit_padded(fb, s->chars, n, fs, true);
}

// ----- chars -----

static void format_char(FmtBuf *fb, int64_t cp, FormatSpec *fs)
{
    if (cp < 0)
        cp = 0;
    if (cp > 0x10FFFF)
        cp = 0xFFFD;

    uint16_t units[2];
    int n = 1;

    if (cp <= 0xFFFF)
    {
        units[0] = (uint16_t)cp;
    }
    else
    {
        uint32_t c = (uint32_t)(cp - 0x10000);
        units[0] = (uint16_t)(0xD800 | (c >> 10));
        units[1] = (uint16_t)(0xDC00 | (c & 0x3FF));
        n = 2;
    }

    emit_padded(fb, units, n, fs, true);
}

// ============================================================
// Public entry point
// ============================================================

ObjString *format_value(ArtState *S, Value v, ObjString *spec)
{
    FormatSpec fs;
    parse_spec(S, spec, &fs);

    // Default the type from the value if the spec doesn't name one.
    if (fs.type == 0)
    {
        if (IS_INT(v))
            fs.type = 'd';
        else if (IS_FLOAT(v))
            fs.type = 'g';
        else
            fs.type = 's';
    }

    FmtBuf fb;
    fb_init(&fb, 64);

    switch (fs.type)
    {
    case 'd':
    case 'i':
    case 'x':
    case 'X':
    case 'o':
    {
        int64_t iv;
        if (IS_INT(v))
            iv = AS_INT(v);
        else if (IS_FLOAT(v))
            iv = (int64_t)AS_FLOAT(v);
        else
        {
            free(fb.buf);
            art_runtime_error(S, NULL,
                              "format: type '%c' expects a number, got %s",
                              fs.type, value_type_name(v));
        }
        format_int(&fb, iv, &fs);
        break;
    }

    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
    {
        double dv;
        if (IS_NUMBER(v))
            dv = AS_NUMBER(v);
        else
        {
            free(fb.buf);
            art_runtime_error(S, NULL,
                              "format: type '%c' expects a number, got %s",
                              fs.type, value_type_name(v));
        }
        format_float(&fb, dv, &fs);
        break;
    }

    case 'c':
    {
        int64_t cp;
        if (IS_INT(v))
            cp = AS_INT(v);
        else if (IS_FLOAT(v))
            cp = (int64_t)AS_FLOAT(v);
        else
        {
            free(fb.buf);
            art_runtime_error(S, NULL,
                              "format: type 'c' expects an int codepoint, got %s",
                              value_type_name(v));
        }
        format_char(&fb, cp, &fs);
        break;
    }

    case 's':
    default:
    {
        // Everything else goes through value_to_string, then the
        // string path. That's how instances, tables, enums, etc.
        // get their rendered form.
        ObjString *vs = value_to_string(S, v);
        format_string(S, &fb, vs, &fs);
        break;
    }
    }

    ObjString *result = obj_string_take_utf16(S, fb.buf, fb.len);
    return result;
}