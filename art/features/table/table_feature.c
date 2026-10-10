// ============================================================
// table_feature.c — the table Feature struct
//
// Claims:
//   register_builtins — installs the Table class
//   to_string         — renders tables as [1, 2, 3] or {"a": 1}
// ============================================================

#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "feature.h"
#include "gc.h"
#include "interp.h"

void art_register_table_builtins(ArtState *S);

// Depth guard for self-referential tables. The runtime is
// single-threaded, so a plain static counter is safe. If a
// render is interrupted by longjmp the counter goes stale, but
// that aborts the whole run anyway.
static int g_render_depth = 0;

// Soft cap on rendered length. Keeps `tostring(giant_array)`
// from producing megabytes by accident. Truncated output ends
// with ", ...".
#define RENDER_CHAR_LIMIT 512

// Wrap a string in quotes with minimal escaping for `"` and `\`.
// Strings inside a rendered table are quoted; top-level strings
// are not, so `tostring("hi")` is still `hi` (no surprise).
static ObjString *quote_string(ArtState *S, ObjString *s)
{
    int cap = s->unit_count * 2 + 2;
    uint16_t *buf = malloc(sizeof(uint16_t) * cap);
    int n = 0;
    buf[n++] = '"';
    for (int i = 0; i < s->unit_count; i++)
    {
        uint16_t c = s->chars[i];
        if (c == '"' || c == '\\')
        {
            buf[n++] = '\\';
            buf[n++] = c;
        }
        else
        {
            buf[n++] = c;
        }
    }
    buf[n++] = '"';
    return obj_string_take_utf16(S, buf, n);
}

// Render an element. Strings get quoted; everything else uses
// value_to_string, which for a nested table re-enters this
// feature's to_string hook and hits the depth guard.
static ObjString *render_element(ArtState *S, Value v)
{
    if (IS_STRING(v))
        return quote_string(S, AS_STRING(v));
    return value_to_string(S, v);
}

static ObjString *render_table(ArtState *S, ObjTable *t)
{
    bool has_arr = t->array_count > 0;
    bool has_hash = t->hash_count > 0;

    if (!has_arr && !has_hash)
        return obj_string_from_utf8(S, "[]", 2);

    // Collect pieces, then concat once. Building an accumulator
    // with obj_string_concat in a loop is O(n²) AND leaves every
    // intermediate result in the intern table permanently.
    int cap = 16;
    int count = 0;
    ObjString **pieces = malloc(sizeof(ObjString *) * cap);

#define PUSH_PIECE(p)                                            \
    do                                                           \
    {                                                            \
        if (count >= cap)                                        \
        {                                                        \
            cap *= 2;                                            \
            pieces = realloc(pieces, sizeof(ObjString *) * cap); \
        }                                                        \
        pieces[count++] = (p);                                   \
    } while (0)

    PUSH_PIECE(obj_string_from_utf8(S, has_arr ? "[" : "{", 1));

    int total_len = 0;
    bool first = true;
    bool truncated = false;

    for (int i = 0; i < t->array_count && !truncated; i++)
    {
        if (!first)
        {
            PUSH_PIECE(obj_string_from_utf8(S, ", ", 2));
            total_len += 2;
        }
        first = false;

        ObjString *elem = render_element(S, t->array[i]);
        PUSH_PIECE(elem);
        total_len += elem->unit_count;

        if (total_len > RENDER_CHAR_LIMIT)
            truncated = true;
    }

    if (!truncated)
    {
        for (int i = 0; i < t->hash_capacity && !truncated; i++)
        {
            TableEntry *e = &t->entries[i];
            if (e->key == NULL)
                continue;

            if (!first)
            {
                PUSH_PIECE(obj_string_from_utf8(S, ", ", 2));
                total_len += 2;
            }
            first = false;

            ObjString *key = quote_string(S, e->key);
            PUSH_PIECE(key);
            total_len += key->unit_count;

            PUSH_PIECE(obj_string_from_utf8(S, ": ", 2));
            total_len += 2;

            ObjString *val = render_element(S, e->value);
            PUSH_PIECE(val);
            total_len += val->unit_count;

            if (total_len > RENDER_CHAR_LIMIT)
                truncated = true;
        }
    }

    if (truncated)
        PUSH_PIECE(obj_string_from_utf8(S, ", ...", 5));

    PUSH_PIECE(obj_string_from_utf8(S, has_arr ? "]" : "}", 1));

#undef PUSH_PIECE

    ObjString *result = obj_string_concat_all(S, pieces, count);
    free(pieces);
    return result;
}

static bool table_to_string(ArtState *S, Value v, ObjString **out)
{
    if (!IS_TABLE(v))
        return false;

    if (g_render_depth >= 4)
    {
        *out = obj_string_from_utf8(S, "[...]", 5);
        return true;
    }

    g_render_depth++;
    ObjString *r = render_table(S, AS_TABLE(v));
    g_render_depth--;

    *out = r;
    return true;
}

static void table_register_builtins(ArtState *S)
{
    art_register_table_builtins(S);
}

Feature table_feature = {
    .name = "table",
    .register_builtins = table_register_builtins,
    .to_string = table_to_string,
};
