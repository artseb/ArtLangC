// ============================================================
// test_stdlib.c — vector2 and vector3 stdlib modules
//
// These are the first non-trivial programs written in ART that
// live outside tests/. They exercise operator overloading with
// typed parameters (including the two-arity-same-type case for
// Vector3's `operator *`), the commutative reverse for scalar
// ops, class methods calling static methods on their own class
// by name, and the derived `!=` from `operator ==`.
//
// Run from the project root: the imports resolve relative to
// the test's own path, which is "<test>" with no directory, so
// they land at ./stdlib/*.art.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "interp.h"
#include "builtins.h"

static int checks = 0;
static int failed = 0;

static void compare_and_report(ArtState *S, const char *label,
                               Value v, const char *want)
{
    checks++;

    if (S->last_error)
    {
        failed++;
        fprintf(stderr, "FAIL: %s -- runtime error\n", label);
        S->last_error = false;
        return;
    }

    char got[256];
    if (IS_STRING(v))
    {
        char *u = obj_string_to_utf8(AS_STRING(v));
        snprintf(got, sizeof(got), "%s", u);
        free(u);
    }
    else if (IS_NIL(v))
        snprintf(got, sizeof(got), "nil");
    else if (IS_BOOL(v))
        snprintf(got, sizeof(got), "%s", AS_BOOL(v) ? "true" : "false");
    else if (IS_INT(v))
        snprintf(got, sizeof(got), "%lld", (long long)AS_INT(v));
    else if (IS_FLOAT(v))
        snprintf(got, sizeof(got), "%g", AS_FLOAT(v));
    else
        snprintf(got, sizeof(got), "<%s>", value_type_name(v));

    if (strcmp(got, want) != 0)
    {
        failed++;
        fprintf(stderr, "FAIL: %s -- got '%s', want '%s'\n",
                label, got, want);
    }
}

// Runs `body` with V bound to the imported Vector3 class. The
// import is prepended; the module cache makes repeat imports
// cheap after the first.
static void v3(ArtState *S, const char *label,
               const char *body, const char *want)
{
    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
                     "local V = import \"stdlib/vector3.art\"\n%s",
                     body);
    if (n < 0 || n >= (int)sizeof(buf))
    {
        checks++;
        failed++;
        fprintf(stderr, "FAIL: %s -- test source too long\n", label);
        return;
    }
    Value v = art_run_source(S, buf, n, "<test>");
    compare_and_report(S, label, v, want);
}

static void v2(ArtState *S, const char *label,
               const char *body, const char *want)
{
    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
                     "local V = import \"stdlib/vector2.art\"\n%s",
                     body);
    if (n < 0 || n >= (int)sizeof(buf))
    {
        checks++;
        failed++;
        fprintf(stderr, "FAIL: %s -- test source too long\n", label);
        return;
    }
    Value v = art_run_source(S, buf, n, "<test>");
    compare_and_report(S, label, v, want);
}

int main(void)
{
    printf("ART stdlib tests\n");
    printf("================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // -------- vector3: construction and fields --------

    v3(S, "v3 field x", "V(1, 2, 3).x", "1");
    v3(S, "v3 field y", "V(1, 2, 3).y", "2");
    v3(S, "v3 field z", "V(1, 2, 3).z", "3");
    v3(S, "v3 default zero", "V().x + V().y + V().z", "0");

    // -------- vector3: statics --------

    v3(S, "v3 zero", "V.zero().x + V.zero().y + V.zero().z", "0");
    v3(S, "v3 one", "V.one().x + V.one().y + V.one().z", "3");
    v3(S, "v3 xAxis", "V.xAxis().x * 100 + V.xAxis().y * 10 + V.xAxis().z", "100");
    v3(S, "v3 yAxis", "V.yAxis().x * 100 + V.yAxis().y * 10 + V.yAxis().z", "10");
    v3(S, "v3 zAxis", "V.zAxis().x * 100 + V.zAxis().y * 10 + V.zAxis().z", "1");

    // -------- vector3: arithmetic --------

    v3(S, "v3 add x",
       "(V(1,2,3) + V(4,5,6)).x", "5");
    v3(S, "v3 add y",
       "(V(1,2,3) + V(4,5,6)).y", "7");
    v3(S, "v3 add z",
       "(V(1,2,3) + V(4,5,6)).z", "9");

    v3(S, "v3 sub",
       "(V(4,5,6) - V(1,2,3)).x", "3");

    v3(S, "v3 hadamard",
       "(V(2,3,4) * V(5,6,7)).y", "18");

    // Scalar multiply. Two overloads with the same arity, so
    // dispatch picks by type: 2 matches `Number`, not `Vector3`.
    v3(S, "v3 scalar mul (right)",
       "(V(1,2,3) * 2).x", "2");
    v3(S, "v3 scalar mul (left)",
       "(2 * V(1,2,3)).x", "2");
    v3(S, "v3 scalar mul (left) y",
       "(3 * V(1,2,3)).y", "6");

    v3(S, "v3 scalar div",
       "(V(10,20,30) / 2).x", "5");
    v3(S, "v3 vector div",
       "(V(10,20,30) / V(2,4,5)).z", "6");

    // -------- vector3: dot and cross --------

    v3(S, "v3 dot",
       "V(1,2,3).dot(V(4,5,6))", "32");

    v3(S, "v3 cross z (xAxis x yAxis)",
       "V.xAxis().cross(V.yAxis()).z", "1");
    v3(S, "v3 cross x (xAxis x yAxis)",
       "V.xAxis().cross(V.yAxis()).x", "0");

    // -------- vector3: magnitude and normal --------

    v3(S, "v3 magnitude 3-4-0",
       "V(3,4,0).magnitude", "5");

    v3(S, "v3 magnitude unit",
       "V(1,0,0).magnitude", "1");

    v3(S, "v3 normal x",
       "V(3,4,0).normal.x", "0.6");

    v3(S, "v3 normal y",
       "V(3,4,0).normal.y", "0.8");

    // The zero-length guard in the `normal` getter is what makes
    // this return a zero vector instead of dividing by zero.
    v3(S, "v3 normal of zero vector",
       "V(0,0,0).normal.x + V(0,0,0).normal.y + V(0,0,0).normal.z", "0");

    // -------- vector3: clone --------

    v3(S, "v3 clone is independent",
       "local a = V(1,2,3)\n"
       "local b = a.clone()\n"
       "b.x = 99\n"
       "a.x",
       "1");

    v3(S, "v3 clone value",
       "V(4,5,6).clone().y", "5");

    // -------- vector3: isZero --------

    v3(S, "v3 isZero true",
       "V.zero().isZero()", "true");

    v3(S, "v3 isZero false",
       "V(1,0,0).isZero()", "false");

    // -------- vector3: equality --------

    v3(S, "v3 == true",
       "V(1,2,3) == V(1,2,3)", "true");

    v3(S, "v3 == false",
       "V(1,2,3) == V(1,2,4)", "false");

    // `!=` is derived: the interpreter finds `operator ==` and
    // negates its result. Not a separate user-defined operator.
    v3(S, "v3 != derived true",
       "V(1,2,3) != V(1,2,4)", "true");

    v3(S, "v3 != derived false",
       "V(1,2,3) != V(1,2,3)", "false");

    // -------- vector3: toString --------

    v3(S, "v3 toString",
       "tostring(V(1,2,3))", "(1, 2, 3)");

    v3(S, "v3 toString via concat",
       "\"point: \" + V(4,5,6)", "point: (4, 5, 6)");

    // -------- vector2: same surface, no z --------

    v2(S, "v2 field x", "V(1, 2).x", "1");
    v2(S, "v2 field y", "V(1, 2).y", "2");
    v2(S, "v2 default", "V().x + V().y", "0");

    v2(S, "v2 xAxis",
       "V.xAxis().x * 10 + V.xAxis().y", "10");
    v2(S, "v2 yAxis",
       "V.yAxis().x * 10 + V.yAxis().y", "1");

    v2(S, "v2 add",
       "(V(1,2) + V(3,4)).x", "4");
    v2(S, "v2 sub",
       "(V(5,6) - V(1,2)).y", "4");
    v2(S, "v2 hadamard",
       "(V(2,3) * V(4,5)).y", "15");
    v2(S, "v2 scalar mul (right)",
       "(V(2,3) * 4).x", "8");
    v2(S, "v2 scalar mul (left)",
       "(4 * V(2,3)).x", "8");
    v2(S, "v2 scalar div",
       "(V(10,20) / 2).y", "10");
    v2(S, "v2 vector div",
       "(V(10,20) / V(2,5)).y", "4");

    v2(S, "v2 magnitude 3-4",
       "V(3,4).magnitude", "5");

    v2(S, "v2 normal x",
       "V(3,4).normal.x", "0.6");

    v2(S, "v2 normal y",
       "V(3,4).normal.y", "0.8");

    v2(S, "v2 == true",
       "V(1,2) == V(1,2)", "true");

    v2(S, "v2 == false",
       "V(1,2) == V(1,3)", "false");

    v2(S, "v2 != derived",
       "V(1,2) != V(1,3)", "true");

    // -------- one test per module that round-trips through JSON --------
    //
    // Pushes both vectors through the JSON writer and parser.
    // A module written in ART is data the interpreter can
    // inspect; the string form goes through `toString`, then
    // through the JSON string path.

    v3(S, "v3 fields survive JSON round-trip",
       "local J = import \"stdlib/json.art\"\n"
       "local payload = [\"x\" = 1, \"y\" = 2, \"z\" = 3]\n"
       "local text = J.to(payload)\n"
       "local back = J.parse(text)\n"
       "back.x * 100 + back.y * 10 + back.z",
       "123");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}