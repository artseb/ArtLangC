// ============================================================
// string_feature.c — the string Feature struct
//
// Claims:
//   value_hash  — the string hash is precomputed at intern time
//   binop       — `+` with a string operand coerces the other
//                 side and concatenates
//               — `*` with one string and one int repeats
// ============================================================

#include <stdlib.h>
#include <string.h>

#include "string.h"
#include "feature.h"
#include "gc.h"
#include "interp.h"

void art_register_string_builtins(ArtState *S);

static bool string_value_hash(Value v, uint32_t *out)
{
    if (!IS_STRING(v))
        return false;
    *out = AS_STRING(v)->hash;
    return true;
}

// Binop hook for strings.
//
// `+` — if either operand is a string, coerce the other via
//       value_to_string and concatenate. `"n = " + 5` gives
//       `"n = 5"`.
//
// `*` — if exactly one operand is a string and the other is an
//       int, repeat. `"ab" * 3` and `3 * "ab"` both give
//       `"ababab"`. n <= 0 gives "". Floats and string digits
//       do NOT work: `"5" * 2` errors, it does not give 10.
//       This is deliberate — no JS-style numeric coercion.
//
// Returns false when neither rule applies, letting core's numeric
// arithmetic handle `1 + 2` and `3 * 4`.
//
// Placed in the string feature rather than core so the coercion
// rules live next to the type they belong to. The class feature
// runs first, so a user-defined `operator +` or `operator *` on
// an instance still wins over either rule.
static bool string_binop(ArtState *S, Node *at, TokenType op,
                         Value a, Value b, Value *out)
{
    if (op == TOKEN_PLUS)
    {
        if (!IS_STRING(a) && !IS_STRING(b))
            return false;

        GC_PUSH(S, a);
        GC_PUSH(S, b);

        ObjString *sa = IS_STRING(a) ? AS_STRING(a) : value_to_string(S, a);
        ObjString *sb = IS_STRING(b) ? AS_STRING(b) : value_to_string(S, b);
        ObjString *result = obj_string_concat(S, sa, sb);

        GC_POP(S, 2);

        *out = OBJ_VAL(result);
        return true;
    }

    if (op == TOKEN_STAR)
    {
        ObjString *s = NULL;
        int64_t n = 0;

        if (IS_STRING(a) && IS_INT(b))
        {
            s = AS_STRING(a);
            n = AS_INT(b);
        }
        else if (IS_INT(a) && IS_STRING(b))
        {
            s = AS_STRING(b);
            n = AS_INT(a);
        }
        else
        {
            return false;
        }

        if (n <= 0 || s->unit_count == 0)
        {
            *out = OBJ_VAL(obj_string_from_utf8(S, "", 0));
            return true;
        }

        // Guard against accidental OOM. 1 << 26 units is 128 MiB
        // of uint16 storage, which is far past any legitimate
        // repeat and catches the runaway case with a clean error
        // instead of a freeze.
        int64_t total = (int64_t)s->unit_count * n;
        if (total > (1 << 26))
            art_runtime_error(S, at, "string repeat result too large");

        GC_PUSH(S, a);
        GC_PUSH(S, b);

        uint16_t *buf = malloc(sizeof(uint16_t) * (size_t)total);
        for (int64_t i = 0; i < n; i++)
            memcpy(buf + i * s->unit_count, s->chars,
                   sizeof(uint16_t) * s->unit_count);

        ObjString *result = obj_string_take_utf16(S, buf, (int)total);
        GC_POP(S, 2);

        *out = OBJ_VAL(result);
        return true;
    }

    return false;
}

static void string_register_builtins(ArtState *S)
{
    art_register_string_builtins(S);
}

Feature string_feature = {
    .name = "string",
    .register_builtins = string_register_builtins,
    .value_hash = string_value_hash,
    .binop = string_binop,
};
