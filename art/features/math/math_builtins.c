// ============================================================
// math_builtins.c — the Math namespace
// ============================================================

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "math.h"
#include "feature.h"
#include "state.h"
#include "value.h"
#include "gc.h"
#include "register.h"

static uint64_t rng_next(ArtState *S)
{
    uint64_t x = S->rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    S->rng_state = x;
    return x;
}

static double rng_double(ArtState *S)
{
    return (rng_next(S) >> 11) * (1.0 / 9007199254740992.0);
}

static Value math_abs(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0]))
        return INT_VAL(llabs(AS_INT(argv[0])));
    return FLOAT_VAL(fabs(AS_NUMBER(argv[0])));
}
static Value math_floor(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0])) return argv[0];
    return FLOAT_VAL(floor(AS_NUMBER(argv[0])));
}
static Value math_ceil(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0])) return argv[0];
    return FLOAT_VAL(ceil(AS_NUMBER(argv[0])));
}
static Value math_round(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0])) return argv[0];
    return FLOAT_VAL(round(AS_NUMBER(argv[0])));
}
static Value math_trunc(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0])) return argv[0];
    return FLOAT_VAL(trunc(AS_NUMBER(argv[0])));
}
static Value math_sqrt(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(sqrt(AS_NUMBER(argv[0])));
}
static Value math_pow(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(pow(AS_NUMBER(argv[0]), AS_NUMBER(argv[1])));
}
static Value math_exp(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(exp(AS_NUMBER(argv[0])));
}
static Value math_log(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(log(AS_NUMBER(argv[0])));
}
static Value math_log2(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(log2(AS_NUMBER(argv[0])));
}
static Value math_log10(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(log10(AS_NUMBER(argv[0])));
}
static Value math_sin(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(sin(AS_NUMBER(argv[0])));
}
static Value math_cos(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(cos(AS_NUMBER(argv[0])));
}
static Value math_tan(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(tan(AS_NUMBER(argv[0])));
}
static Value math_asin(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(asin(AS_NUMBER(argv[0])));
}
static Value math_acos(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(acos(AS_NUMBER(argv[0])));
}
static Value math_atan(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(atan(AS_NUMBER(argv[0])));
}
static Value math_atan2(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(atan2(AS_NUMBER(argv[0]), AS_NUMBER(argv[1])));
}
static Value math_min(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0]) && IS_INT(argv[1]))
        return INT_VAL(AS_INT(argv[0]) < AS_INT(argv[1])
                           ? AS_INT(argv[0]) : AS_INT(argv[1]));
    double a = AS_NUMBER(argv[0]), b = AS_NUMBER(argv[1]);
    return FLOAT_VAL(a < b ? a : b);
}
static Value math_max(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0]) && IS_INT(argv[1]))
        return INT_VAL(AS_INT(argv[0]) > AS_INT(argv[1])
                           ? AS_INT(argv[0]) : AS_INT(argv[1]));
    double a = AS_NUMBER(argv[0]), b = AS_NUMBER(argv[1]);
    return FLOAT_VAL(a > b ? a : b);
}
static Value math_clamp(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0]) && IS_INT(argv[1]) && IS_INT(argv[2]))
    {
        int64_t v = AS_INT(argv[0]);
        int64_t lo = AS_INT(argv[1]);
        int64_t hi = AS_INT(argv[2]);
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        return INT_VAL(v);
    }
    double v = AS_NUMBER(argv[0]);
    double lo = AS_NUMBER(argv[1]);
    double hi = AS_NUMBER(argv[2]);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return FLOAT_VAL(v);
}
static Value math_sign(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    if (IS_INT(argv[0]))
    {
        int64_t v = AS_INT(argv[0]);
        return INT_VAL(v > 0 ? 1 : (v < 0 ? -1 : 0));
    }
    double v = AS_FLOAT(argv[0]);
    return INT_VAL(v > 0 ? 1 : (v < 0 ? -1 : 0));
}
static Value math_deg(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(AS_NUMBER(argv[0]) * (180.0 / 3.14159265358979323846));
}
static Value math_rad(ArtState *S, int argc, Value *argv)
{
    (void)S; (void)argc;
    return FLOAT_VAL(AS_NUMBER(argv[0]) * (3.14159265358979323846 / 180.0));
}
static Value math_random(ArtState *S, int argc, Value *argv)
{
    double r = rng_double(S);
    if (argc == 0)
        return FLOAT_VAL(r);
    if (argc == 1)
        return FLOAT_VAL(r * AS_NUMBER(argv[0]));
    double lo = AS_NUMBER(argv[0]);
    double hi = AS_NUMBER(argv[1]);
    return FLOAT_VAL(lo + r * (hi - lo));
}
static Value math_seed(ArtState *S, int argc, Value *argv)
{
    (void)argc;
    S->rng_state = (uint64_t)AS_INT(argv[0]);
    if (S->rng_state == 0)
        S->rng_state = 0x9E3779B97F4A7C15ull;
    return NIL_VAL;
}

static void math_register_builtins(ArtState *S)
{
    ObjTable *math = obj_table_new(S);
    GC_PUSH(S, OBJ_VAL(math));

    art_define_value(S, math, "pi",  FLOAT_VAL(3.14159265358979323846));
    art_define_value(S, math, "tau", FLOAT_VAL(6.28318530717958647692));
    art_define_value(S, math, "e",   FLOAT_VAL(2.71828182845904523536));
    art_define_value(S, math, "inf", FLOAT_VAL(HUGE_VAL));
    art_define_value(S, math, "nan", FLOAT_VAL(NAN));

    art_define_native(S, math, "abs",    math_abs,    1);
    art_define_native(S, math, "floor",  math_floor,  1);
    art_define_native(S, math, "ceil",   math_ceil,   1);
    art_define_native(S, math, "round",  math_round,  1);
    art_define_native(S, math, "trunc",  math_trunc,  1);
    art_define_native(S, math, "sqrt",   math_sqrt,   1);
    art_define_native(S, math, "pow",    math_pow,    2);
    art_define_native(S, math, "exp",    math_exp,    1);
    art_define_native(S, math, "log",    math_log,    1);
    art_define_native(S, math, "log2",   math_log2,   1);
    art_define_native(S, math, "log10",  math_log10,  1);
    art_define_native(S, math, "sin",    math_sin,    1);
    art_define_native(S, math, "cos",    math_cos,    1);
    art_define_native(S, math, "tan",    math_tan,    1);
    art_define_native(S, math, "asin",   math_asin,   1);
    art_define_native(S, math, "acos",   math_acos,   1);
    art_define_native(S, math, "atan",   math_atan,   1);
    art_define_native(S, math, "atan2",  math_atan2,  2);
    art_define_native(S, math, "min",    math_min,    2);
    art_define_native(S, math, "max",    math_max,    2);
    art_define_native(S, math, "clamp",  math_clamp,  3);
    art_define_native(S, math, "sign",   math_sign,   1);
    art_define_native(S, math, "deg",    math_deg,    1);
    art_define_native(S, math, "rad",    math_rad,    1);
    art_define_native(S, math, "random", math_random, -1);
    art_define_native(S, math, "seed",   math_seed,   1);

    math->frozen = true;

    art_define_global(S, "Math", OBJ_VAL(math));

    GC_POP(S, 1);
}

Feature math_feature = {
    .name = "math",
    .register_builtins = math_register_builtins,
};
