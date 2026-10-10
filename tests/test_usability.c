// ============================================================
// test_usability.c — negative indexing, table rendering,
// bare-name suggestions, and keyword-as-member-name
// ============================================================

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
    printf("ART usability tests\n");
    printf("===================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // ============================================================
    // Negative indexing
    // ============================================================

    expect(S, "t[-1] is last",        "[10, 20, 30][-1]", "30");
    expect(S, "t[-2] is second-last", "[10, 20, 30][-2]", "20");
    expect(S, "t[-3] is first",       "[10, 20, 30][-3]", "10");
    expect(S, "t[-4] out of range",   "[10, 20, 30][-4]", "nil");
    expect(S, "t[-1] on empty is nil","[][-1]",           "nil");

    expect(S, "t[-1] = v sets last",
           "local t = [1, 2, 3]\n"
           "t[-1] = 99\n"
           "t[3]",
           "99");

    expect(S, "t[-2] = v sets second-to-last",
           "local t = [1, 2, 3]\n"
           "t[-2] = 88\n"
           "t[2]",
           "88");

    expect_error(S, "t[-10] = v on short table errors",
                 "local t = [1, 2]\n"
                 "t[-10] = 5\n");

    expect(S, "negative index in arithmetic",
           "local t = [10, 20, 30]\n"
           "t[-1] * 2",
           "60");

    // ============================================================
    // Table to_string
    // ============================================================

    expect(S, "empty table", "tostring([])", "[]");

    expect(S, "array of ints",
           "tostring([1, 2, 3])",
           "[1, 2, 3]");

    expect(S, "hash table",
           "tostring([\"a\" = 1, \"b\" = 2])",
           "{\"a\": 1, \"b\": 2}");

    expect(S, "nested arrays",
           "tostring([[1, 2], [3, 4]])",
           "[[1, 2], [3, 4]]");

    expect(S, "strings inside are quoted",
           "tostring([\"hi\", \"there\"])",
           "[\"hi\", \"there\"]");

    expect(S, "table via concat",
           "\"got \" + [1, 2, 3]",
           "got [1, 2, 3]");

    expect(S, "table via interpolation",
           "local t = [1, 2, 3]\n"
           "\"values: ${t}\"",
           "values: [1, 2, 3]");

    expect(S, "mixed array and hash",
           "tostring([1, 2, \"name\" = \"Bob\"])",
           "[1, 2, \"name\": \"Bob\"]");

    // ============================================================
    // Bare-name suggestion inside a method
    // ============================================================

    expect(S, "bare field name resolves to this.name",
           "class A {\n"
           "    name = \"x\"\n"
           "    fun A() { }\n"
           "    fun fetch() { return name }\n"
           "}\n"
           "A().fetch()",
           "x");

    expect(S, "bare method name resolves to this.method",
           "class A {\n"
           "    fun A() { }\n"
           "    fun hello() { return \"hi\" }\n"
           "    fun fetch() { return hello() }\n"
           "}\n"
           "A().fetch()",
           "hi");

    expect(S, "undefined variable outside method is nil",
           "definitely_not_defined_anywhere",
           "nil");

    expect(S, "local shadows field",
           "class A {\n"
           "    name = \"field\"\n"
           "    fun A() { }\n"
           "    fun fetch() { local name = \"local\"\n return name }\n"
           "}\n"
           "A().fetch()",
           "local");

    expect(S, "param shadows field",
           "class A {\n"
           "    name = \"field\"\n"
           "    fun A() { }\n"
           "    fun say(name) { return name }\n"
           "}\n"
           "A().say(\"param\")",
           "param");

    // ============================================================
    // Keyword as member name
    //
    // `get` and `set` are keywords, but after a `.` there's no
    // ambiguity, so they can be used as method names. The parser
    // accepts any word-like token in that position.
    // ============================================================

    expect(S, "method named 'get' is callable",
           "class A {\n"
           "    value = 42\n"
           "    fun A() { }\n"
           "    fun get() { return this.value }\n"
           "}\n"
           "A().get()",
           "42");

    expect(S, "method named 'set' is callable",
           "class A {\n"
           "    value = 0\n"
           "    fun A() { }\n"
           "    fun set(v) { this.value = v }\n"
           "}\n"
           "local a = A()\n"
           "a.set(99)\n"
           "a.value",
           "99");

    expect(S, "method named 'static' is callable",
           "class A {\n"
           "    fun A() { }\n"
           "    fun static() { return \"static\" }\n"
           "}\n"
           "A().static()",
           "static");

    expect(S, "method named 'class' is callable",
           "class A {\n"
           "    fun A() { }\n"
           "    fun class() { return \"class\" }\n"
           "}\n"
           "A().class()",
           "class");

    expect(S, "hash key via keyword-named method",
           "local t = [\"get\" = 5]\n"
           "t.get",
           "5");

    expect_error(S, "member access on non-word token still errors",
                 "local x = 5\n"
                 "x.{");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
