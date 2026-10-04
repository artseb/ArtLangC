// ============================================================
// string_methods.c — the String class methods
// ============================================================

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "string.h"
#include "state.h"
#include "value.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "register.h"

static Value get_this(ArtState *S)
{
    Value v;
    if (!art_scope_lookup(S->scope, S->this_name, NULL, &v))
        v = NIL_VAL;
    return v;
}

static Value take_string(ArtState *S, uint16_t *units, int unit_count)
{
    ObjString *r = obj_string_take_utf16(S, units, unit_count);
    return OBJ_VAL(r);
}

static uint16_t *copy_units(ObjString *s)
{
    if (s->unit_count == 0)
        return NULL;
    uint16_t *buf = malloc(sizeof(uint16_t) * s->unit_count);
    memcpy(buf, s->chars, sizeof(uint16_t) * s->unit_count);
    return buf;
}

static Value string_length(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;
    return INT_VAL(obj_string_length(AS_STRING(self)));
}

static Value string_upper(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    uint16_t *buf = copy_units(s);
    for (int i = 0; i < s->unit_count; i++)
    {
        uint16_t c = buf[i];
        if (c >= 'a' && c <= 'z')
            buf[i] = c - 'a' + 'A';
    }
    return take_string(S, buf, s->unit_count);
}

static Value string_lower(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    uint16_t *buf = copy_units(s);
    for (int i = 0; i < s->unit_count; i++)
    {
        uint16_t c = buf[i];
        if (c >= 'A' && c <= 'Z')
            buf[i] = c - 'A' + 'a';
    }
    return take_string(S, buf, s->unit_count);
}

static Value string_is_empty(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return BOOL_VAL(true);
    return BOOL_VAL(AS_STRING(self)->unit_count == 0);
}

static Value string_char_at(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    int i = (int)AS_INT(argv[0]);
    int n = obj_string_length(s);

    if (i < 1 || i > n) return NIL_VAL;

    ObjString *r = obj_string_substring(S, s, i, i);
    return OBJ_VAL(r);
}

static Value string_substring(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    int a = (int)AS_INT(argv[0]);
    int b = (int)AS_INT(argv[1]);
    int n = obj_string_length(s);

    if (a < 1) a = 1;
    if (b > n + 1) b = n + 1;
    if (a > b) { int t = a; a = b; b = t; }

    if (a >= b)
        return OBJ_VAL(obj_string_from_utf8(S, "", 0));

    ObjString *r = obj_string_substring(S, s, a, b - 1);
    return OBJ_VAL(r);
}

static bool is_ascii_space(uint16_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static Value string_trim(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    int start = 0;
    int end = s->unit_count;

    while (start < end && is_ascii_space(s->chars[start])) start++;
    while (end > start && is_ascii_space(s->chars[end - 1])) end--;

    int len = end - start;
    uint16_t *buf = malloc(sizeof(uint16_t) * (len == 0 ? 1 : len));
    memcpy(buf, s->chars + start, sizeof(uint16_t) * len);
    return take_string(S, buf, len);
}

static Value string_to_int(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    char *utf8 = obj_string_to_utf8(AS_STRING(self));
    char *end = NULL;
    long long v = strtoll(utf8, &end, 10);

    bool ok = (end != utf8);
    if (ok)
    {
        while (*end == ' ' || *end == '\t' ||
               *end == '\n' || *end == '\r') end++;
        if (*end != '\0') ok = false;
    }

    free(utf8);
    if (!ok) return NIL_VAL;
    return INT_VAL(v);
}

static Value string_to_float(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    char *utf8 = obj_string_to_utf8(AS_STRING(self));
    char *end = NULL;
    double v = strtod(utf8, &end);

    bool ok = (end != utf8);
    if (ok)
    {
        while (*end == ' ' || *end == '\t' ||
               *end == '\n' || *end == '\r') end++;
        if (*end != '\0') ok = false;
    }

    free(utf8);
    if (!ok) return NIL_VAL;
    return FLOAT_VAL(v);
}

static Value string_starts_with(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return BOOL_VAL(false);
    if (!IS_STRING(argv[0])) return BOOL_VAL(false);

    ObjString *s = AS_STRING(self);
    ObjString *p = AS_STRING(argv[0]);
    if (p->unit_count > s->unit_count) return BOOL_VAL(false);

    return BOOL_VAL(memcmp(s->chars, p->chars,
                           sizeof(uint16_t) * p->unit_count) == 0);
}

static Value string_ends_with(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return BOOL_VAL(false);
    if (!IS_STRING(argv[0])) return BOOL_VAL(false);

    ObjString *s = AS_STRING(self);
    ObjString *p = AS_STRING(argv[0]);
    if (p->unit_count > s->unit_count) return BOOL_VAL(false);

    int off = s->unit_count - p->unit_count;
    return BOOL_VAL(memcmp(s->chars + off, p->chars,
                           sizeof(uint16_t) * p->unit_count) == 0);
}

static Value string_contains(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return BOOL_VAL(false);
    if (!IS_STRING(argv[0])) return BOOL_VAL(false);

    ObjString *s = AS_STRING(self);
    ObjString *p = AS_STRING(argv[0]);

    if (p->unit_count == 0) return BOOL_VAL(true);
    if (p->unit_count > s->unit_count) return BOOL_VAL(false);

    int limit = s->unit_count - p->unit_count;
    for (int i = 0; i <= limit; i++)
    {
        if (memcmp(s->chars + i, p->chars,
                   sizeof(uint16_t) * p->unit_count) == 0)
            return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

static Value string_index_of(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;
    if (!IS_STRING(argv[0])) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *p = AS_STRING(argv[0]);
    int start_unit = 0;

    if (argc >= 2)
    {
        int start_cp = (int)AS_INT(argv[1]);
        if (start_cp < 1) start_cp = 1;
        if (start_cp > s->unit_count + 1) return NIL_VAL;

        int cp = 1, u = 0;
        while (u < s->unit_count && cp < start_cp)
        {
            uint16_t c = s->chars[u];
            u += (c >= 0xD800 && c <= 0xDBFF) ? 2 : 1;
            cp++;
        }
        start_unit = u;
    }

    if (p->unit_count == 0) return INT_VAL(start_unit + 1);
    if (p->unit_count + start_unit > s->unit_count) return NIL_VAL;

    int limit = s->unit_count - p->unit_count;
    for (int i = start_unit; i <= limit; i++)
    {
        if (memcmp(s->chars + i, p->chars,
                   sizeof(uint16_t) * p->unit_count) == 0)
        {
            int cp = 1;
            for (int j = 0; j < i;)
            {
                uint16_t c = s->chars[j];
                j += (c >= 0xD800 && c <= 0xDBFF) ? 2 : 1;
                cp++;
            }
            return INT_VAL(cp);
        }
    }
    return NIL_VAL;
}

static Value string_repeat(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    int n = (int)AS_INT(argv[0]);
    if (n < 0) n = 0;

    ObjString *s = AS_STRING(self);
    if (n == 0 || s->unit_count == 0)
        return OBJ_VAL(obj_string_from_utf8(S, "", 0));

    int total = s->unit_count * n;
    uint16_t *buf = malloc(sizeof(uint16_t) * total);
    for (int i = 0; i < n; i++)
        memcpy(buf + i * s->unit_count, s->chars,
               sizeof(uint16_t) * s->unit_count);

    return take_string(S, buf, total);
}

static Value string_reverse(ArtState *S, int argc, Value *argv)
{
    (void)argc; (void)argv;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    if (s->unit_count == 0)
        return OBJ_VAL(obj_string_from_utf8(S, "", 0));

    uint16_t *buf = malloc(sizeof(uint16_t) * s->unit_count);
    int w = 0;
    int r = s->unit_count;

    while (r > 0)
    {
        int cp_start = r - 1;
        if (cp_start > 0 &&
            s->chars[cp_start] >= 0xDC00 && s->chars[cp_start] <= 0xDFFF &&
            s->chars[cp_start - 1] >= 0xD800 && s->chars[cp_start - 1] <= 0xDBFF)
        {
            cp_start--;
        }

        int cp_len = r - cp_start;
        memcpy(buf + w, s->chars + cp_start, sizeof(uint16_t) * cp_len);
        w += cp_len;
        r = cp_start;
    }

    return take_string(S, buf, s->unit_count);
}

// "a-b-c".replace("-", "+") -> "a+b+c"
// All occurrences, literal match (no regex). Empty needle is a
// no-op — Python's behavior, and avoids the "infinite match"
// problem that would otherwise need special-casing.
static Value string_replace(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;
    if (!IS_STRING(argv[0])) return NIL_VAL;
    if (!IS_STRING(argv[1])) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *needle = AS_STRING(argv[0]);
    ObjString *repl = AS_STRING(argv[1]);

    if (needle->unit_count == 0)
        return OBJ_VAL(s);

    // Worst-case growth: every unit is the start of a match and
    // the replacement is longer. Allocate for that, then hand the
    // buffer to obj_string_take_utf16 with the actual length.
    int max_matches = s->unit_count / needle->unit_count;
    int cap = s->unit_count;
    if (repl->unit_count > needle->unit_count)
        cap += max_matches * (repl->unit_count - needle->unit_count);
    if (cap < 1) cap = 1;

    uint16_t *buf = malloc(sizeof(uint16_t) * cap);
    int n = 0;

    int i = 0;
    int limit = s->unit_count - needle->unit_count;
    while (i <= limit)
    {
        if (memcmp(s->chars + i, needle->chars,
                   sizeof(uint16_t) * needle->unit_count) == 0)
        {
            memcpy(buf + n, repl->chars,
                   sizeof(uint16_t) * repl->unit_count);
            n += repl->unit_count;
            i += needle->unit_count;
        }
        else
        {
            buf[n++] = s->chars[i];
            i++;
        }
    }
    while (i < s->unit_count)
    {
        buf[n++] = s->chars[i];
        i++;
    }

    return OBJ_VAL(obj_string_take_utf16(S, buf, n));
}

static Value string_split(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    Value self = get_this(S);
    if (!IS_STRING(self)) return NIL_VAL;
    if (!IS_STRING(argv[0])) return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *sep = AS_STRING(argv[0]);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    if (sep->unit_count == 0)
    {
        int i = 0;
        while (i < s->unit_count)
        {
            int len = 1;
            if (s->chars[i] >= 0xD800 && s->chars[i] <= 0xDBFF &&
                i + 1 < s->unit_count)
                len = 2;

            ObjString *piece = obj_string_substring(S, s, i + 1, i + len);
            GC_PUSH(S, OBJ_VAL(piece));
            table_push(S, out, OBJ_VAL(piece));
            GC_POP(S, 1);
            i += len;
        }
        GC_POP(S, 1);
        return OBJ_VAL(out);
    }

    int start = 0;
    int i = 0;
    int limit = s->unit_count - sep->unit_count;

    while (i <= limit)
    {
        if (memcmp(s->chars + i, sep->chars,
                   sizeof(uint16_t) * sep->unit_count) == 0)
        {
            ObjString *piece = obj_string_substring(S, s,
                                                    start + 1, i);
            GC_PUSH(S, OBJ_VAL(piece));
            table_push(S, out, OBJ_VAL(piece));
            GC_POP(S, 1);

            i += sep->unit_count;
            start = i;
        }
        else
        {
            i++;
        }
    }

    ObjString *tail;
    if (start >= s->unit_count)
        tail = obj_string_from_utf8(S, "", 0);
    else
        tail = obj_string_substring(S, s, start + 1, s->unit_count);
    GC_PUSH(S, OBJ_VAL(tail));
    table_push(S, out, OBJ_VAL(tail));
    GC_POP(S, 1);

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

void art_register_string_builtins(ArtState *S)
{
    ObjString *name = obj_string_from_utf8(S, "String", 6);
    GC_PUSH(S, OBJ_VAL(name));
    ObjClass *klass = obj_class_new(S, name, NULL);
    GC_PUSH(S, OBJ_VAL(klass));

    S->builtin_classes[OBJ_STRING] = klass;

    art_define_method(S, klass, "length",     string_length,       0);
    art_define_method(S, klass, "upper",      string_upper,        0);
    art_define_method(S, klass, "lower",      string_lower,        0);
    art_define_method(S, klass, "isEmpty",    string_is_empty,     0);
    art_define_method(S, klass, "charAt",     string_char_at,      1);
    art_define_method(S, klass, "substring",  string_substring,    2);
    art_define_method(S, klass, "trim",       string_trim,         0);
    art_define_method(S, klass, "toInt",      string_to_int,       0);
    art_define_method(S, klass, "toFloat",    string_to_float,     0);
    art_define_method(S, klass, "startsWith", string_starts_with,  1);
    art_define_method(S, klass, "endsWith",   string_ends_with,    1);
    art_define_method(S, klass, "contains",   string_contains,     1);
    art_define_method(S, klass, "indexOf",    string_index_of,    -1);
    art_define_method(S, klass, "repeat",     string_repeat,       1);
    art_define_method(S, klass, "reverse",    string_reverse,      0);
    art_define_method(S, klass, "replace",    string_replace,      2);
    art_define_method(S, klass, "split",      string_split,        1);

    GC_POP(S, 2);
}
