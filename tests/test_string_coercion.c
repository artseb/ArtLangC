// ============================================================
// test_string_coercion.c — `+` and `*` with a string operand
//
// `+`: if either operand is a string, the other is converted
//      via value_to_string and the results are concatenated.
//
// `*`: if exactly one operand is a string and the other is an
//      int, the string repeats. This applies regardless of what
//      the string contains — "5" * 5 is "55555", not 25. The
//      digit-ness is irrelevant; it's just a one-character
//      string being repeated.
//
//      What does NOT work: floats on either side, and strings on
//      both sides. "ab" * 2.5 and "ab" * "3" are errors. This is
//      the boundary that keeps ART out of JS-coercion territory.
//
// Numeric `+` and `*` are unchanged. User-defined operators on
// instances still win over coercion.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "interp.h"
#include "builtins.h"

static int checks = 0;
static int failed = 0;

static void expect(ArtState *S, const char *label,
                   const char *src, const char *want)
{
       Value v = art_run_source(S, src, (int)strlen(src), "<test>");
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

static void expect_error(ArtState *S, const char *label, const char *src)
{
       art_run_source(S, src, (int)strlen(src), "<test>");
       checks++;
       if (!S->last_error)
       {
              failed++;
              fprintf(stderr, "FAIL: %s -- expected runtime error, got none\n",
                      label);
       }
       S->last_error = false;
}

int main(void)
{
       printf("ART string coercion tests\n");
       printf("=========================\n\n");

       ArtState *S = art_state_new();
       art_register_builtins(S);

       // -------- `+` string on the left --------

       expect(S, "string + int", "\"n = \" + 5", "n = 5");
       expect(S, "string + float", "\"x = \" + 1.5", "x = 1.5");
       expect(S, "string + true", "\"on: \" + true", "on: true");
       expect(S, "string + false", "\"on: \" + false", "on: false");
       expect(S, "string + nil", "\"v: \" + nil", "v: nil");

       // -------- `+` string on the right --------

       expect(S, "int + string", "5 + \" items\"", "5 items");
       expect(S, "float + string", "1.5 + \"g\"", "1.5g");
       expect(S, "true + string", "true + \"!\"", "true!");

       // -------- `+` existing behavior preserved --------

       expect(S, "string + string still concats", "\"foo\" + \"bar\"", "foobar");
       expect(S, "int + int is still arithmetic", "1 + 2", "3");
       expect(S, "float + int is still arithmetic", "1.5 + 1", "2.5");

       // -------- `+` instances use their toString --------

       expect(S, "string + instance with toString",
              "class V { fun V() { } fun toString() { return \"V\" } }\n"
              "\"got \" + V()",
              "got V");

       expect(S, "string + instance without toString",
              "class Bare { fun Bare() { } }\n"
              "\"got \" + Bare()",
              "got <Bare instance>");

       // -------- `*` repeat --------

       expect(S, "string * int", "\"ab\" * 3", "ababab");
       expect(S, "int * string", "3 * \"ab\"", "ababab");
       expect(S, "string * 1", "\"x\" * 1", "x");
       expect(S, "string * 0 is empty", "\"ab\" * 0", "");
       expect(S, "string * negative is empty", "\"ab\" * -1", "");
       expect(S, "empty string * n", "\"\" * 5", "");

       // Digit strings repeat like any other string. "5" * 5 is
       // "55555", not 25. The contents of the string never matter.
       expect(S, "digit string * int repeats", "\"5\" * 5", "55555");
       expect(S, "int * digit string repeats", "3 * \"7\"", "777");

       expect(S, "multichar repeat", "\"=\" * 20",
              "====================");

       expect(S, "repeat in interpolation",
              "local n = 3\n"
              "\"${\"-\" * n}\"",
              "---");

       // -------- `*` boundaries: no JS-style coercion --------

       expect_error(S, "string * float does not repeat",
                    "\"ab\" * 2.5");

       expect_error(S, "float * string does not repeat",
                    "2.5 * \"ab\"");

       expect_error(S, "string * string is an error",
                    "\"ab\" * \"3\"");

       // -------- numeric `*` unchanged --------

       expect(S, "int * int still arithmetic", "3 * 4", "12");
       expect(S, "float * int still arithmetic", "1.5 * 2", "3");

       // -------- user-defined operator still wins --------

       expect(S, "user operator + on the left wins over coercion",
              "class V {\n"
              "    fun V() { }\n"
              "    operator +(o) { return \"custom +\" }\n"
              "}\n"
              "V() + \"x\"",
              "custom +");

       expect(S, "user operator * on the right wins over repeat",
              "class V {\n"
              "    fun V() { }\n"
              "    operator *(s) { return \"custom *\" }\n"
              "}\n"
              "\"x\" * V()",
              "custom *");

       // -------- non-`+`/`*` operators still error --------

       expect_error(S, "string minus int is an error", "\"a\" - 1");
       expect_error(S, "string divided by int is an error", "\"a\" / 2");
       expect_error(S, "string modulo int is an error", "\"a\" % 2");

       art_state_free(S);

       printf("\n%d checks, %d failed\n", checks, failed);
       return failed == 0 ? 0 : 1;
}
