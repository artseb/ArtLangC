// ============================================================
// string_runtime.c — UTF-16 string creation, interning, conversion
// ============================================================

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "value.h"
#include "state.h"

static uint32_t hash_utf16(const uint16_t *units, int count)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < count; i++)
    {
        hash ^= units[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint16_t *utf8_to_utf16(const char *utf8, int byte_len, int *out_count)
{
    uint16_t *out = malloc(sizeof(uint16_t) * (byte_len + 1));
    if (!out)
        return NULL;

    int n = 0;
    int i = 0;

    while (i < byte_len)
    {
        uint8_t b0 = (uint8_t)utf8[i];
        uint32_t cp = 0;
        int extra = 0;

        if (b0 < 0x80)
        {
            cp = b0;
            extra = 0;
        }
        else if ((b0 & 0xE0) == 0xC0)
        {
            cp = b0 & 0x1F;
            extra = 1;
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            cp = b0 & 0x0F;
            extra = 2;
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            cp = b0 & 0x07;
            extra = 3;
        }
        else
        {
            out[n++] = 0xFFFD;
            i++;
            continue;
        }

        if (i + extra >= byte_len)
        {
            out[n++] = 0xFFFD;
            break;
        }

        bool ok = true;
        for (int k = 1; k <= extra; k++)
        {
            uint8_t bx = (uint8_t)utf8[i + k];
            if ((bx & 0xC0) != 0x80)
            {
                ok = false;
                break;
            }
            cp = (cp << 6) | (bx & 0x3F);
        }

        if (!ok)
        {
            out[n++] = 0xFFFD;
            i++;
            continue;
        }

        i += extra + 1;

        if (cp <= 0xFFFF)
        {
            out[n++] = (uint16_t)cp;
        }
        else if (cp <= 0x10FFFF)
        {
            cp -= 0x10000;
            out[n++] = (uint16_t)(0xD800 | (cp >> 10));
            out[n++] = (uint16_t)(0xDC00 | (cp & 0x3FF));
        }
        else
        {
            out[n++] = 0xFFFD;
        }
    }

    *out_count = n;
    return out;
}

static char *utf16_to_utf8(const uint16_t *units, int count, int *out_byte_len)
{
    char *out = malloc(count * 4 + 1);
    if (!out)
        return NULL;

    int b = 0;
    for (int i = 0; i < count; i++)
    {
        uint32_t cp = units[i];

        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < count)
        {
            uint16_t lo = units[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
        }

        if (cp < 0x80)
        {
            out[b++] = (char)cp;
        }
        else if (cp < 0x800)
        {
            out[b++] = (char)(0xC0 | (cp >> 6));
            out[b++] = (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out[b++] = (char)(0xE0 | (cp >> 12));
            out[b++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[b++] = (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out[b++] = (char)(0xF0 | (cp >> 18));
            out[b++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[b++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[b++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    out[b] = '\0';
    if (out_byte_len)
        *out_byte_len = b;
    return out;
}

static int utf16_char_count(const uint16_t *units, int count)
{
    int n = 0;
    for (int i = 0; i < count; i++)
    {
        uint16_t u = units[i];
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < count)
        {
            uint16_t lo = units[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF)
                i++;
        }
        n++;
    }
    return n;
}

static int utf16_advance(const uint16_t *units, int count, int start_unit, int n_chars)
{
    int i = start_unit;
    while (n_chars > 0 && i < count)
    {
        uint16_t u = units[i];
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < count)
        {
            uint16_t lo = units[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                i += 2;
                n_chars--;
                continue;
            }
        }
        i++;
        n_chars--;
    }
    return i;
}

ObjString *obj_string_new_utf16(ArtState *S, const uint16_t *units, int unit_count)
{
    ObjString *str = ALLOCATE_OBJ(S, ObjString, OBJ_STRING);
    str->unit_count = unit_count;
    str->char_count = -1;
    str->chars = art_realloc(S, NULL, 0, sizeof(uint16_t) * (unit_count + 1));
    memcpy(str->chars, units, sizeof(uint16_t) * unit_count);
    str->chars[unit_count] = 0;
    str->hash = hash_utf16(str->chars, unit_count);
    return str;
}

ObjString *obj_string_take_utf16(ArtState *S, uint16_t *units, int unit_count)
{
    ObjString *str = ALLOCATE_OBJ(S, ObjString, OBJ_STRING);
    str->unit_count = unit_count;
    str->char_count = -1;
    str->chars = units;
    str->hash = hash_utf16(units, unit_count);
    return str;
}

ObjString *obj_string_intern_utf16(ArtState *S, const uint16_t *units, int unit_count)
{
    uint32_t hash = hash_utf16(units, unit_count);

    if (S->strings != NULL && S->strings->hash_capacity > 0)
    {
        uint32_t mask = S->strings->hash_capacity - 1;
        uint32_t idx = hash & mask;

        for (;;)
        {
            TableEntry *entry = &S->strings->entries[idx];

            if (entry->key == NULL)
            {
                if (IS_NIL(entry->value))
                    break;
            }
            else if (entry->key->hash == hash && entry->key->unit_count == unit_count && memcmp(entry->key->chars, units, sizeof(uint16_t) * unit_count) == 0)
            {
                return entry->key;
            }

            idx = (idx + 1) & mask;
        }
    }

    ObjString *str = obj_string_new_utf16(S, units, unit_count);
    GC_PUSH(S, OBJ_VAL(str));
    table_set(S, S->strings, str, OBJ_VAL(str));
    GC_POP(S, 1);
    return str;
}

ObjString *obj_string_from_utf8(ArtState *S, const char *utf8, int byte_len)
{
    int unit_count = 0;
    uint16_t *units = utf8_to_utf16(utf8, byte_len, &unit_count);
    if (!units)
        return NULL;
    ObjString *str = obj_string_intern_utf16(S, units, unit_count);
    free(units);
    return str;
}

ObjString *obj_string_from_fmt(ArtState *S, const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n < 0)
        return obj_string_from_utf8(S, "", 0);
    if (n < (int)sizeof(buf))
        return obj_string_from_utf8(S, buf, n);

    char *heap = malloc((size_t)n + 1);
    if (!heap)
        return obj_string_from_utf8(S, "", 0);

    va_start(ap, fmt);
    vsnprintf(heap, (size_t)n + 1, fmt, ap);
    va_end(ap);

    ObjString *result = obj_string_from_utf8(S, heap, n);
    free(heap);
    return result;
}

char *obj_string_to_utf8(ObjString *s)
{
    int byte_len = 0;
    return utf16_to_utf8(s->chars, s->unit_count, &byte_len);
}

int obj_string_length(ObjString *s)
{
    if (s->char_count < 0)
    {
        s->char_count = utf16_char_count(s->chars, s->unit_count);
    }
    return s->char_count;
}

ObjString *obj_string_concat(ArtState *S, ObjString *a, ObjString *b)
{
    GC_PUSH(S, OBJ_VAL(a));
    GC_PUSH(S, OBJ_VAL(b));

    int total = a->unit_count + b->unit_count;
    uint16_t *buf = malloc(sizeof(uint16_t) * total);
    memcpy(buf, a->chars, sizeof(uint16_t) * a->unit_count);
    memcpy(buf + a->unit_count, b->chars, sizeof(uint16_t) * b->unit_count);

    ObjString *result = obj_string_intern_utf16(S, buf, total);
    free(buf);

    GC_POP(S, 2);
    return result;
}

ObjString *obj_string_substring(ArtState *S, ObjString *s,
                                int start_char, int end_char)
{
    if (start_char < 1)
        start_char = 1;
    if (end_char > obj_string_length(s))
        end_char = obj_string_length(s);
    if (end_char < start_char)
    {
        return obj_string_intern_utf16(S, (const uint16_t *)u"", 0);
    }

    int start_unit = utf16_advance(s->chars, s->unit_count, 0, start_char - 1);
    int end_unit = utf16_advance(s->chars, s->unit_count, start_unit,
                                 end_char - start_char + 1);

    GC_PUSH(S, OBJ_VAL(s));
    ObjString *result = obj_string_intern_utf16(
        S, s->chars + start_unit, end_unit - start_unit);
    GC_POP(S, 1);

    return result;
}
