// ============================================================
// test_reflection.c — instance reflection: fields, get, set
//
// Objects expose three methods that let a script walk an
// instance's members dynamically:
//
//   obj.fields()         -> array of accessible member names
//   obj.get(name)        -> value, bound method, or getter result
//   obj.set(name, value) -> sets a field or calls a setter
//
// Private members (declared with `local`) and static members
// are excluded from fields() and error on get/set from outside
// the class. Public fields, public getters, and public methods
// are all visible.
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
    printf("ART reflection tests\n");
    printf("====================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // ============================================================
    // fields() — public members only
    // ============================================================

    expect(S, "fields includes public fields",
           "class V {\n"
           "    x = 1\n"
           "    y = 2\n"
           "    fun V() { }\n"
           "}\n"
           "V().fields().contains(\"x\")",
           "true");

    expect(S, "fields includes second public field",
           "class V {\n"
           "    x = 1\n"
           "    y = 2\n"
           "    fun V() { }\n"
           "}\n"
           "V().fields().contains(\"y\")",
           "true");

    expect(S, "fields excludes private fields",
           "class V {\n"
           "    local secret = 1\n"
           "    pub = 2\n"
           "    fun V() { }\n"
           "}\n"
           "V().fields().contains(\"secret\")",
           "false");

    expect(S, "fields includes methods",
           "class V {\n"
           "    fun V() { }\n"
           "    fun hello() { return 1 }\n"
           "}\n"
           "V().fields().contains(\"hello\")",
           "true");

    expect(S, "fields excludes private methods",
           "class V {\n"
           "    fun V() { }\n"
           "    local fun hidden() { return 1 }\n"
           "}\n"
           "V().fields().contains(\"hidden\")",
           "false");

    expect(S, "fields includes getters",
           "class V {\n"
           "    fun V() { }\n"
           "    get value { return 42 }\n"
           "}\n"
           "V().fields().contains(\"value\")",
           "true");

    expect(S, "fields includes inherited",
           "class A {\n"
           "    fun A() { }\n"
           "    fun parent() { return 1 }\n"
           "}\n"
           "class B extends A {\n"
           "    fun B() { }\n"
           "    fun child() { return 2 }\n"
           "}\n"
           "B().fields().contains(\"parent\")",
           "true");

    expect(S, "fields includes inherited own",
           "class A { fun A() { }\n fun p() { return 1 } }\n"
           "class B extends A { fun B() { }\n fun c() { return 2 } }\n"
           "B().fields().contains(\"c\")",
           "true");

    expect(S, "fields has no duplicates",
           "class A { fun A() { }\n fun p() { return 1 } }\n"
           "class B extends A { fun B() { }\n fun p() { return 2 } }\n"
           "local names = B().fields()\n"
           "local count = 0\n"
           "for (local n in names) { if (n == \"p\") { count = count + 1 } }\n"
           "count",
           "1");

    // ============================================================
    // get() — fields, getters, methods
    // ============================================================

    expect(S, "get field by name",
           "class V {\n"
           "    x = 42\n"
           "    fun V() { }\n"
           "}\n"
           "V().get(\"x\")",
           "42");

    expect(S, "get returns instance field set by constructor",
           "class V {\n"
           "    x = 0\n"
           "    fun V(v) { this.x = v }\n"
           "}\n"
           "V(7).get(\"x\")",
           "7");

    expect(S, "get on getter invokes it",
           "class V {\n"
           "    x = 0\n"
           "    fun V(x) { this.x = x }\n"
           "    get doubled { return this.x * 2 }\n"
           "}\n"
           "V(5).get(\"doubled\")",
           "10");

    expect(S, "get on method returns a bound method",
           "class V {\n"
           "    fun V() { }\n"
           "    fun hello() { return \"hi\" }\n"
           "}\n"
           "V().get(\"hello\")()",
           "hi");

    expect(S, "get returns a callable method",
           "class Calc {\n"
           "    n = 0\n"
           "    fun Calc(n) { this.n = n }\n"
           "    fun twice() { return this.n * 2 }\n"
           "}\n"
           "local m = Calc(21).get(\"twice\")\n"
           "m()",
           "42");

    expect_error(S, "get missing member errors",
                 "class V { fun V() { } }\n"
                 "V().get(\"nope\")");

    expect_error(S, "get private field errors",
                 "class V {\n"
                 "    local secret = 1\n"
                 "    fun V() { }\n"
                 "}\n"
                 "V().get(\"secret\")");

    expect_error(S, "get private method errors",
                 "class V {\n"
                 "    fun V() { }\n"
                 "    local fun hidden() { return 1 }\n"
                 "}\n"
                 "V().get(\"hidden\")");

    expect_error(S, "get non-string key errors",
                 "class V { fun V() { } }\n"
                 "V().get(42)");

    // ============================================================
    // set() — fields and setters
    // ============================================================

    expect(S, "set public field by name",
           "class V {\n"
           "    x = 0\n"
           "    fun V() { }\n"
           "}\n"
           "local v = V()\n"
           "v.set(\"x\", 99)\n"
           "v.x",
           "99");

    expect(S, "set invokes setter",
           "class V {\n"
           "    _x = 0\n"
           "    fun V() { }\n"
           "    set x(v) { this._x = v * 2 }\n"
           "    get x() { return this._x }\n"
           "}\n"
           "local v = V()\n"
           "v.set(\"x\", 5)\n"
           "v.x",
           "10");

    expect_error(S, "set missing member errors",
                 "class V { fun V() { } }\n"
                 "local v = V()\n"
                 "v.set(\"nope\", 1)");

    expect_error(S, "set private field errors",
                 "class V {\n"
                 "    local secret = 0\n"
                 "    fun V() { }\n"
                 "}\n"
                 "local v = V()\n"
                 "v.set(\"secret\", 1)");

    // ============================================================
    // Reflection also works on File instances (builtin class)
    // ============================================================

    // (Skipped — File instances hold a FILE * and have no
    // user-declared fields. Reflection still works structurally,
    // but the test would need a real file to be meaningful.)

    // ============================================================
    // Real use: recursive dump
    // ============================================================

    expect(S, "recursive walk produces the right field count",
           "class Inner {\n"
           "    a = 1\n"
           "    b = 2\n"
           "    fun Inner() { }\n"
           "}\n"
           "class Outer {\n"
           "    x = 0\n"
           "    inner = nil\n"
           "    fun Outer() { this.inner = Inner() }\n"
           "}\n"
           "local o = Outer()\n"
           "local total = 0\n"
           "for (local name in o.fields()) {\n"
           "    local v = o.get(name)\n"
           "    if (v is Inner) {\n"
           "        for (local inner_name in v.fields()) {\n"
           "            total = total + 1\n"
           "        }\n"
           "    }\n"
           "}\n"
           "total",
           "2");

    expect(S, "reflection in a method can read own private fields",
           "class V {\n"
           "    local secret = 42\n"
           "    fun V() { }\n"
           "    fun reveal() { return this.get(\"secret\") }\n"
           "}\n"
           "V().reveal()",
           "42");

    // ============================================================
    // User override of fields() wins
    // ============================================================

    expect(S, "user-defined fields() shadows the built-in",
           "class V {\n"
           "    fun V() { }\n"
           "    fun fields() { return [\"custom\"] }\n"
           "}\n"
           "V().fields()[1]",
           "custom");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
