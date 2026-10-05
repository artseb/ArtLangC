// ============================================================
// string_gsub.c — gsub
//
// Replaces every match of a pattern with a replacement, which
// may be a string (with %0/%1..%9/%% escapes) or a function
// (called with the captures, or the whole match when there are
// none; return value is tostring'd).
//
// Works on a UTF-16 accumulator so no encode/decode round-trip
// is needed — the source is uint16_t, the replacement values
// are uint16_t, and the result is handed straight to
// obj_string_take_utf16.
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

static void grow_u16(uint16_t **buf, int *cap, int *len,
                     const uint16_t *src, int n)
{
    if (*len + n + 1 > *cap)
    {
        while (*len + n + 1 > *cap)
            *cap *= 2;
        *buf = realloc(*buf, sizeof(uint16_t) * *cap);
    }
    memcpy(*buf + *len, src, sizeof(uint16_t) * n);
    *len += n;
}

// Expand a replacement string against one match. Appends the
// result to `out`. `repl` is the raw replacement string (already
// known to be an ObjString). Handles:
//
//   %%   literal %
//   %0   the whole match
//   %1..%9  captures (ignored silently if the index is out of
//           range or refers to an unfinished capture)
//
// Anything else following a % is a runtime error, matching Lua.
static void expand_repl(ArtState *S, ObjString *repl,
                        PatternMatch *m, const uint16_t *src,
                        uint16_t **out, int *cap, int *len)
{
    int i = 0;
    while (i < repl->unit_count)
    {
        uint16_t c = repl->chars[i];

        if (c != '%' || i + 1 >= repl->unit_count)
        {
            grow_u16(out, cap, len, &c, 1);
            i++;
            continue;
        }

        uint16_t nx = repl->chars[i + 1];
        i += 2;

        if (nx == '%')
        {
            uint16_t pct = '%';
            grow_u16(out, cap, len, &pct, 1);
        }
        else if (nx == '0')
        {
            grow_u16(out, cap, len,
                     src + m->match_start, m->match_len);
        }
        else if (nx >= '1' && nx <= '9')
        {
            int idx = nx - '1';
            if (idx < m->ncaptures && m->caps[idx].len >= 0)
            {
                grow_u16(out, cap, len,
                         src + m->caps[idx].start,
                         m->caps[idx].len);
            }
            // Out of range: silently emit nothing, matching Lua.
        }
        else
        {
            art_runtime_error(S, NULL,
                              "invalid use of '%%' in replacement string");
        }
    }
}

// s.gsub(pattern, repl [, n]) -> new string
//
// repl: string with escape sequences, or a callable.
// n:    maximum number of replacements; -1 (or absent) means all.
static Value string_gsub(ArtState *S, int argc, Value *argv)
{
    Value self = get_this(S);
    if (!IS_STRING(self))
        return NIL_VAL;
    if (argc < 2)
        return NIL_VAL;
    if (!IS_STRING(argv[0]))
        return NIL_VAL;

    ObjString *s = AS_STRING(self);
    ObjString *pat = AS_STRING(argv[0]);
    Value repl = argv[1];

    bool repl_is_string = IS_STRING(repl);
    bool repl_is_callable = IS_CALLABLE(repl);
    if (!repl_is_string && !repl_is_callable)
        return NIL_VAL;

    int max_n = -1;
    if (argc >= 3 && IS_INT(argv[2]))
        max_n = (int)AS_INT(argv[2]);

    ObjString *repl_str = repl_is_string ? AS_STRING(repl) : NULL;

    int cap = 128, len = 0;
    uint16_t *out = malloc(sizeof(uint16_t) * cap);

    int pos = 0;
    int done = 0;

    while (pos <= s->unit_count)
    {
        if (max_n >= 0 && done >= max_n)
            break;

        PatternMatch m;
        if (!pattern_match(S, s->chars, s->unit_count,
                           pat->chars, pat->unit_count,
                           pos, &m))
            break;

        // Copy the gap between `pos` and the match.
        if (m.match_start > pos)
            grow_u16(&out, &cap, &len,
                     s->chars + pos, m.match_start - pos);

        // Build the replacement text.
        if (repl_is_callable)
        {
            // Args: all captures, or the whole match if none.
            Value cargs[PAT_MAX_CAPTURES];
            int nargs;

            if (m.ncaptures == 0)
            {
                cargs[0] = OBJ_VAL(obj_string_substring(S, s,
                                                        m.match_start + 1,
                                                        m.match_start + m.match_len));
                nargs = 1;
            }
            else
            {
                for (int i = 0; i < m.ncaptures; i++)
                {
                    PatternCap *c = &m.caps[i];
                    if (c->len == -2)
                    {
                        cargs[i] = INT_VAL(c->start + 1);
                    }
                    else
                    {
                        uint16_t *b = malloc(
                            sizeof(uint16_t) * c->len);
                        memcpy(b, s->chars + c->start,
                               sizeof(uint16_t) * c->len);
                        cargs[i] = OBJ_VAL(
                            obj_string_take_utf16(S, b, c->len));
                    }
                }
                nargs = m.ncaptures;
            }

            Value r = call_any(S, repl, nargs, cargs, NULL);
            if (S->control != CONTROL_NONE)
            {
                free(out);
                return NIL_VAL;
            }

            ObjString *rs = value_to_string(S, r);
            grow_u16(&out, &cap, &len, rs->chars, rs->unit_count);
        }
        else
        {
            expand_repl(S, repl_str, &m, s->chars, &out, &cap, &len);
        }

        done++;
        pos = m.match_start + m.match_len;

        // Zero-length match: emit the current unit and advance
        // one so we don't loop forever on an anchored pattern
        // that matches empty strings.
        if (m.match_len == 0)
        {
            if (m.match_start >= s->unit_count)
                break;
            uint16_t c = s->chars[m.match_start];
            grow_u16(&out, &cap, &len, &c, 1);
            pos = m.match_start + 1;
        }
    }

    // Trailing text.
    if (pos < s->unit_count)
        grow_u16(&out, &cap, &len,
                 s->chars + pos, s->unit_count - pos);

    ObjString *result = obj_string_take_utf16(S, out, len);
    return OBJ_VAL(result);
}

void art_register_gsub_methods(ArtState *S, ObjClass *klass)
{
    art_define_method(S, klass, "gsub", string_gsub, -1);
}