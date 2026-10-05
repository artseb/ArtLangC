#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "interp.h"
#include "features/registry.h"

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
    printf("ART variadic interface tests\n");
    printf("============================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    expect(S, "variadic interface, variadic impl",
           "interface L { fun log(...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(...parts) { return parts.length() }\n"
           "}\n"
           "C().log(1, 2, 3)",
           "3");

    expect(S, "variadic interface, impl can be called with zero",
           "interface L { fun log(...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(...parts) { return parts.length() }\n"
           "}\n"
           "C().log()",
           "0");

    expect(S, "variadic interface, impl with default",
           "interface L { fun log(...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(a = 0, ...parts) { return parts.length() + 1 }\n"
           "}\n"
           "C().log()",
           "1");

    expect_error(S, "variadic interface, fixed impl fails",
                 "interface L { fun log(...parts) }\n"
                 "class C implements L {\n"
                 "    fun C() { }\n"
                 "    fun log(a) { }\n"
                 "}\n");

    expect_error(S, "variadic interface, fixed impl with default fails",
                 "interface L { fun log(...parts) }\n"
                 "class C implements L {\n"
                 "    fun C() { }\n"
                 "    fun log(a = 0) { }\n"
                 "}\n");

    expect_error(S, "variadic interface, impl requires min 1 fails",
                 "interface L { fun log(...parts) }\n"
                 "class C implements L {\n"
                 "    fun C() { }\n"
                 "    fun log(a, ...parts) { }\n"
                 "}\n");

    expect(S, "variadic with fixed prefix, matching impl",
           "interface L { fun log(tag, ...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(tag, ...parts) { return tag + \":\" + tostring(parts.length()) }\n"
           "}\n"
           "C().log(\"x\", 1, 2, 3)",
           "x:3");

    expect(S, "variadic with fixed prefix, zero rest",
           "interface L { fun log(tag, ...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(tag, ...parts) { return tag + \":\" + tostring(parts.length()) }\n"
           "}\n"
           "C().log(\"x\")",
           "x:0");

    expect(S, "variadic with fixed prefix, impl with default",
           "interface L { fun log(tag, ...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(tag, other = 0, ...parts) { return tostring(parts.length()) }\n"
           "}\n"
           "C().log(\"x\", 5)",
           "0");

    expect_error(S, "variadic with fixed prefix, impl min too high fails",
                 "interface L { fun log(tag, ...parts) }\n"
                 "class C implements L {\n"
                 "    fun C() { }\n"
                 "    fun log(a, b, ...parts) { }\n"
                 "}\n");

    expect_error(S, "variadic with fixed prefix, fixed impl fails",
                 "interface L { fun log(tag, ...parts) }\n"
                 "class C implements L {\n"
                 "    fun C() { }\n"
                 "    fun log(a, b) { }\n"
                 "}\n");

    expect(S, "mixed fixed and variadic requirements",
           "interface Logger {\n"
           "    fun info(msg)\n"
           "    fun trace(...parts)\n"
           "}\n"
           "class L implements Logger {\n"
           "    fun L() { }\n"
           "    fun info(msg) { return \"info: \" + msg }\n"
           "    fun trace(...parts) { return \"trace: \" + tostring(parts.length()) }\n"
           "}\n"
           "local l = L()\n"
           "l.info(\"hi\") + \" / \" + l.trace(1, 2)",
           "info: hi / trace: 2");

    expect_error(S, "mixed, missing variadic half",
                 "interface Logger {\n"
                 "    fun info(msg)\n"
                 "    fun trace(...parts)\n"
                 "}\n"
                 "class L implements Logger {\n"
                 "    fun L() { }\n"
                 "    fun info(msg) { }\n"
                 "}\n");

    expect(S, "inherited variadic requirement",
           "interface A { fun ping(...parts) }\n"
           "interface B extends A { fun pong() }\n"
           "class C implements B {\n"
           "    fun C() { }\n"
           "    fun ping(...parts) { return parts.length() }\n"
           "    fun pong() { return \"pong\" }\n"
           "}\n"
           "C().ping(1, 2, 3, 4)",
           "4");

    expect_error(S, "inherited variadic requirement unsatisfied",
                 "interface A { fun ping(...parts) }\n"
                 "interface B extends A { fun pong() }\n"
                 "class C implements B {\n"
                 "    fun C() { }\n"
                 "    fun ping(a) { }\n"
                 "    fun pong() { }\n"
                 "}\n");

    expect(S, "overload set with one variadic satisfies",
           "interface L { fun log(...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(msg) { return \"fixed\" }\n"
           "    fun log(...parts) { return \"variadic\" }\n"
           "}\n"
           "C().log(1, 2, 3)",
           "variadic");

    expect(S, "interface type annotation accepts variadic impl",
           "interface L { fun log(...parts) }\n"
           "class C implements L {\n"
           "    fun C() { }\n"
           "    fun log(...parts) { return parts.length() }\n"
           "}\n"
           "fun call(l: L) { return l.log(1, 2, 3) }\n"
           "call(C())",
           "3");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}