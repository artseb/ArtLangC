// ============================================================
// string_find.c — find, match, gmatch
//
// Thin wrappers over the pattern engine in core/syntax/pattern.c.
// Each one calls pattern_match and converts the resulting
// PatternMatch into Value shapes.
// ============================================================

#include <stdlib.h>
#include <string.h>

#include "string.h"
#include "state.h"
#include "value.h"
#include "interp.h"
#include "scope.h"
#include "gc.h"
#include "register.h"
#include "pattern.h"

static Value get_this(ArtState *S)
{
    Value v;
    if (!art_scope_lookup(S->scope, S->this_name, NULL, &v))
        v = NIL_VAL;
    return v;
}

// init defaults to 1 (1-based, Lua convention). Clamped to the
// valid range [1, len+1].
static int parse_init(ObjString *s, Value *argv, int argc, int arg_i)
{
    int init = 1;
    if (argc > arg_i && IS_INT(argv[arg_i]))
        init = (int)AS_INT(argv[arg_i]);
    if (init < 1)
        init = 1;
    if (init > s->unit_count + 1)
        init = s->unit_count + 1;
    return init;
}

// Build a Value from one capture.
// Position captures become 1-based ints. Substring captures
// become ObjStrings.
static Value cap_to_value(ArtState *S, const uint16_t *src, PatternCap *c)
{
    if (c->len == -2)
        return INT_VAL(c->start + 1);

    uint16_t *buf = malloc((size_t)c->len * sizeof(uint16_t));
    memcpy(buf, src + c->start, (size_t)c->len * sizeof(uint16_t));
    return OBJ_VAL(obj_string_take_utf16(S, buf, c->len));
}

// Build an ObjString from a match span. `start` is 0-based,
// `len` is unit count. obj_string_substring is 1-based inclusive,
// so the span becomes [start+1, start+len].
static Value span_to_string(ArtState *S, ObjString *s, int start, int len)
{
    if (len == 0)
        return OBJ_VAL(obj_string_from_utf8(S, "", 0));
    return OBJ_VAL(obj_string_substring(S, s, start + 1, start + len));
}

// s.find(pattern [, init]) -> [start, end, cap1, ...] or nil
static Value string_find(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_STRING(self))
        return NIL_VAL;
    if (argc < 1 || !IS_STRING(argv[0]))
        return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *pat = AS_STRING(argv[0]);
    int init = parse_init(s, argv, argc, 1);

    PatternMatch m;
    if (!pattern_match(S, s->chars, s->unit_count,
                       pat->chars, pat->unit_count,
                       init - 1, &m))
        return NIL_VAL;

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    table_push(S, out, INT_VAL(m.match_start + 1));
    table_push(S, out, INT_VAL(m.match_start + m.match_len));

    for (int i = 0; i < m.ncaptures; i++)
    {
        Value cv = cap_to_value(S, s->chars, &m.caps[i]);
        GC_PUSH(S, cv);
        table_push(S, out, cv);
        GC_POP(S, 1);
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

// s.match(pattern [, init]) -> the match, or nil
//
// No captures  : the whole matched substring
// One capture  : that capture
// Two+ captures: a table of captures
static Value string_match(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_STRING(self))
        return NIL_VAL;
    if (argc < 1 || !IS_STRING(argv[0]))
        return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *pat = AS_STRING(argv[0]);
    int init = parse_init(s, argv, argc, 1);

    PatternMatch m;
    if (!pattern_match(S, s->chars, s->unit_count,
                       pat->chars, pat->unit_count,
                       init - 1, &m))
        return NIL_VAL;

    if (m.ncaptures == 0)
        return span_to_string(S, s, m.match_start, m.match_len);

    if (m.ncaptures == 1)
        return cap_to_value(S, s->chars, &m.caps[0]);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));
    for (int i = 0; i < m.ncaptures; i++)
    {
        Value cv = cap_to_value(S, s->chars, &m.caps[i]);
        GC_PUSH(S, cv);
        table_push(S, out, cv);
        GC_POP(S, 1);
    }
    GC_POP(S, 1);
    return OBJ_VAL(out);
}

// s.gmatch(pattern) -> table of every match
//
// No captures  : each element is the matched substring
// One+ captures: each element is a table of captures
//
// Iteration is eager; the whole result is built up front.
// Zero-length matches advance one unit to avoid an infinite loop.
static Value string_gmatch(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_STRING(self))
        return NIL_VAL;
    if (argc < 1 || !IS_STRING(argv[0]))
        return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *pat = AS_STRING(argv[0]);

    ObjTable *out = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(out));

    int pos = 0;
    while (pos <= s->unit_count)
    {
        PatternMatch m;
        if (!pattern_match(S, s->chars, s->unit_count,
                           pat->chars, pat->unit_count,
                           pos, &m))
            break;

        if (m.ncaptures == 0)
        {
            Value v = span_to_string(S, s, m.match_start, m.match_len);
            GC_PUSH(S, v);
            table_push(S, out, v);
            GC_POP(S, 1);
        }
        else
        {
            ObjTable *row = obj_table_new(S);
            GC_PUSH(S, OBJ_VAL(row));
            for (int i = 0; i < m.ncaptures; i++)
            {
                Value cv = cap_to_value(S, s->chars, &m.caps[i]);
                GC_PUSH(S, cv);
                table_push(S, row, cv);
                GC_POP(S, 1);
            }
            table_push(S, out, OBJ_VAL(row));
            GC_POP(S, 1);
        }

        pos = (m.match_len == 0)
                  ? m.match_start + 1
                  : m.match_start + m.match_len;
    }

    GC_POP(S, 1);
    return OBJ_VAL(out);
}

void art_register_find_methods(ArtState *S, ObjClass *klass)
{
    art_define_method(S, klass, "find", string_find, -1);
    art_define_method(S, klass, "match", string_match, -1);
    art_define_method(S, klass, "gmatch", string_gmatch, 1);
}