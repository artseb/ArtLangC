// ============================================================
// test_errors.c — structured error values, error() with any
// argument type, and the round-trip through attempt()
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "interp.h"

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

    char got[512];
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
    printf("ART error tests\n");
    printf("===============\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // ============================================================
    // error() accepts any value
    // ============================================================

    expect(S, "error with string is caught",
           "local ok = attempt(fun() { error(\"boom\") })[1]\n"
           "ok",
           "false");

    expect(S, "error with table is caught",
           "local ok = attempt(fun() { error([\"code\" = 404]) })[1]\n"
           "ok",
           "false");

    expect(S, "error with int is caught",
           "local ok = attempt(fun() { error(42) })[1]\n"
           "ok",
           "false");

    expect(S, "error with nil is caught",
           "local ok = attempt(fun() { error(nil) })[1]\n"
           "ok",
           "false");

    expect(S, "error with bool is caught",
           "local ok = attempt(fun() { error(false) })[1]\n"
           "ok",
           "false");

    // ============================================================
    // attempt hands back the thrown value unchanged
    // ============================================================

    expect(S, "string payload preserved",
           "local _, msg = attempt(fun() { error(\"specific message\") })\n"
           "msg",
           "specific message");

    expect(S, "table payload preserved",
           "local _, info = attempt(fun() {\n"
           "    error([\"code\" = 404, \"reason\" = \"not found\"])\n"
           "})\n"
           "info.code",
           "404");

    expect(S, "table payload second field",
           "local _, info = attempt(fun() {\n"
           "    error([\"code\" = 404, \"reason\" = \"not found\"])\n"
           "})\n"
           "info.reason",
           "not found");

    expect(S, "int payload preserved",
           "local _, code = attempt(fun() { error(42) })\n"
           "code",
           "42");

    expect(S, "bool payload preserved",
           "local _, flag = attempt(fun() { error(false) })\n"
           "flag",
           "false");

    expect(S, "nil payload preserved",
           "local _, v = attempt(fun() { error(nil) })\n"
           "v == nil",
           "true");

    expect(S, "instance payload preserved",
           "class E {\n"
           "    code = 0\n"
           "    fun E(c) { this.code = c }\n"
           "}\n"
           "local _, err = attempt(fun() { error(E(500)) })\n"
           "err.code",
           "500");

    // ============================================================
    // interpreter errors still produce string payloads
    // ============================================================

    expect(S, "division by zero produces a string",
           "local _, msg = attempt(fun() { 1 / 0 })\n"
           "msg is String",
           "true");

    expect(S, "division by zero message mentions the cause",
           "local _, msg = attempt(fun() { 1 / 0 })\n"
           "msg.contains(\"division\")",
           "true");

    expect(S, "undefined method error is a string",
           "class A { fun A() { } }\n"
           "local _, msg = attempt(fun() { A().nope() })\n"
           "msg is String",
           "true");

    expect(S, "nil member access error is a string",
           "local _, msg = attempt(fun() {\n"
           "    local x = nil\n"
           "    return x.field\n"
           "})\n"
           "msg is String",
           "true");

    // ============================================================
    // nested attempt — the outer catch sees the outer error
    // ============================================================

    expect(S, "inner error doesn't leak to outer",
           "local ok, v = attempt(fun() {\n"
           "    local inner = attempt(fun() { error(\"inner\") })\n"
           "    return \"outer\"\n"
           "})\n"
           "v",
           "outer");

    expect(S, "outer error after inner still caught",
           "local _, msg = attempt(fun() {\n"
           "    local inner = attempt(fun() { error(\"inner\") })\n"
           "    error(\"outer\")\n"
           "})\n"
           "msg",
           "outer");

    // ============================================================
    // uncaught errors still go to the top level
    // ============================================================

    expect_error(S, "uncaught error propagates",
                 "error(\"uncaught\")");

    expect_error(S, "uncaught table error propagates",
                 "error([\"code\" = 500])");

    // ============================================================
    // state recovers cleanly after any error
    // ============================================================

    expect(S, "state usable after caught error",
           "local _ = attempt(fun() { error(42) })\n"
           "1 + 1",
           "2");

    expect(S, "state usable after uncaught-and-recovered error",
           "1 + 1",
           "2");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
