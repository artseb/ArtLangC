// ============================================================
// test_sugar.c — if expression, variadic push, multi-assign,
// string replace, and hash-value contains
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
    printf("ART sugar tests\n");
    printf("===============\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // ============================================================
    // if expression
    // ============================================================

    expect(S, "if expr true branch",
           "local x = if (true) -> \"yes\" else \"no\"\n"
           "x",
           "yes");

    expect(S, "if expr false branch",
           "local x = if (false) -> \"yes\" else \"no\"\n"
           "x",
           "no");

    expect(S, "if expr with condition",
           "local n = 7\n"
           "local s = if (n > 5) -> \"big\" else \"small\"\n"
           "s",
           "big");

    expect(S, "if expr with condition false",
           "local n = 2\n"
           "local s = if (n > 5) -> \"big\" else \"small\"\n"
           "s",
           "small");

    expect(S, "if expr with arithmetic branches",
           "local n = 10\n"
           "if (n > 5) -> n * 2 else n + 100",
           "20");

    expect(S, "if expr nested",
           "local n = 3\n"
           "local s = if (n == 1) -> \"one\"\n"
           "          else if (n == 2) -> \"two\"\n"
           "          else \"many\"\n"
           "s",
           "many");

    expect(S, "if expr returns nil without else",
           "local x = if (false) -> 5\n"
           "x",
           "nil");

    // The statement form still works and its value is discarded
    // when used at statement level.
    expect(S, "if statement form still works",
           "local msg = \"\"\n"
           "if (true) { msg = \"yes\" } else { msg = \"no\" }\n"
           "msg",
           "yes");

    expect(S, "else if chain at statement level",
           "fun f(n) {\n"
           "    if (n == 1) { return \"one\" }\n"
           "    else if (n == 2) { return \"two\" }\n"
           "    else { return \"many\" }\n"
           "}\n"
           "f(2)",
           "two");

    // ============================================================
    // Variadic push
    // ============================================================

    expect(S, "push one arg",
           "local t = [1]\n"
           "t.push(2)\n"
           "t.length()",
           "2");

    expect(S, "push multiple args",
           "local t = [1]\n"
           "t.push(2, 3, 4)\n"
           "t.length()",
           "4");

    expect(S, "push values preserved",
           "local t = []\n"
           "t.push(\"a\", \"b\", \"c\")\n"
           "t.join(\"\")",
           "abc");

    expect(S, "push zero args is no-op",
           "local t = [1, 2]\n"
           "t.push()\n"
           "t.length()",
           "2");

    expect(S, "push returns self",
           "local t = []\n"
           "t.push(1).push(2).push(3)\n"
           "t.length()",
           "3");

    // ============================================================
    // Multi-assignment (a, b = expr)
    // ============================================================

    expect(S, "multi-assign basic",
           "local a = 0\n"
           "local b = 0\n"
           "a, b = [10, 20]\n"
           "a * 100 + b",
           "1020");

    expect(S, "multi-assign from function result",
           "fun pair() { return [1, 2] }\n"
           "local x = 0\n"
           "local y = 0\n"
           "x, y = pair()\n"
           "x + y",
           "3");

    expect(S, "multi-assign three names",
           "local a = 0\n"
           "local b = 0\n"
           "local c = 0\n"
           "a, b, c = [7, 8, 9]\n"
           "a + b + c",
           "24");

    expect(S, "multi-assign missing slots become nil",
           "local a = 0\n"
           "local b = 99\n"
           "a, b = [1]\n"
           "b",
           "nil");

    expect(S, "multi-assign extra slots ignored",
           "local a = 0\n"
           "local b = 0\n"
           "a, b = [1, 2, 3, 4]\n"
           "a * 10 + b",
           "12");

    expect(S, "multi-assign in loop",
           "local a = 0\n"
           "local b = 0\n"
           "for (local i = 1 -> 3) {\n"
           "    a, b = [i, i * 10]\n"
           "}\n"
           "a * 100 + b",
           "330");

    // Declaration form still works.
    expect(S, "local multi-decl still works",
           "local a, b, c = [1, 2, 3]\n"
           "a + b + c",
           "6");

    // ============================================================
    // String replace
    // ============================================================

    expect(S, "replace one occurrence",
           "\"hello world\".replace(\"world\", \"art\")",
           "hello art");

    expect(S, "replace multiple occurrences",
           "\"a-b-c-d\".replace(\"-\", \"+\")",
           "a+b+c+d");

    expect(S, "replace with empty",
           "\"hello\".replace(\"l\", \"\")",
           "heo");

    expect(S, "replace with longer",
           "\"a.b.c\".replace(\".\", \"::\")",
           "a::b::c");

    expect(S, "replace needle not present",
           "\"hello\".replace(\"xyz\", \"!\")",
           "hello");

    expect(S, "replace empty needle is no-op",
           "\"hello\".replace(\"\", \"X\")",
           "hello");

    expect(S, "replace whole string",
           "\"aaa\".replace(\"aaa\", \"b\")",
           "b");

    // ============================================================
    // Table contains checks hash values
    // ============================================================

    expect(S, "contains array value",
           "[1, 2, 3].contains(2)",
           "true");

    expect(S, "contains hash value",
           "local t = [\"a\" = 10, \"b\" = 20]\n"
           "t.contains(20)",
           "true");

    expect(S, "contains hash value miss",
           "local t = [\"a\" = 10]\n"
           "t.contains(99)",
           "false");

    expect(S, "contains mixed",
           "local t = [1, 2, \"x\" = 3]\n"
           "t.contains(3)",
           "true");

    // The key is not a value; `in` checks keys, `contains` doesn't.
    expect(S, "contains does not match key",
           "local t = [\"key\" = \"value\"]\n"
           "t.contains(\"key\")",
           "false");

    expect(S, "in still checks keys",
           "local t = [\"key\" = \"value\"]\n"
           "\"key\" in t",
           "true");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
