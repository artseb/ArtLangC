// ============================================================
// test_bugfixes.c — regression tests for bugs found during the
// refactor audit. Each test names the bug it guards against.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "interp.h"
#include "features/registry.h"
#include "gc.h"

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

    char got[128];
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

int main(void)
{
    printf("ART regression tests\n");
    printf("====================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // -------- Bug 1: RNG was zeroed by memset in state_new ----
    expect(S, "Math.random() nonzero without seed",
           "Math.random() != 0",
           "true");

    expect(S, "Math.random() varies across calls",
           "local a = Math.random()\n"
           "local b = Math.random()\n"
           "local c = Math.random()\n"
           "a != b or b != c",
           "true");

    expect(S, "Math.random() in [0,1)",
           "local r = Math.random()\n"
           "r >= 0 and r < 1",
           "true");

    // -------- Bug 2: GC didn't mark ObjFunction param types -----
    expect(S, "typed param function: setup",
           "class V {\n"
           "    fun V() { }\n"
           "}\n"
           "fun f(a: V) { return a is V }\n"
           "f(V())",
           "true");

    art_gc_collect(S);
    art_gc_collect(S);

    expect(S, "typed param function survives GC",
           "class V { fun V() { } }\n"
           "fun f(a: V) { return a is V }\n"
           "f(V())",
           "true");

    art_gc_collect(S);

    expect(S, "typed param function survives more GC",
           "class V { fun V() { } }\n"
           "fun f(a: V) { return a is V }\n"
           "f(V())",
           "true");

    // -------- Bug 3: class interfaces weren't marked -------------
    expect(S, "class implements interface",
           "interface D { fun d() }\n"
           "class C implements D {\n"
           "    fun C() { }\n"
           "    fun d() { return 1 }\n"
           "}\n"
           "C() is D",
           "true");

    art_gc_collect(S);
    art_gc_collect(S);

    expect(S, "interface check survives GC",
           "C() is D",
           "true");

    // -------- Bug 4: default_value nodes were leaked -------------
    // Run the same program many times to stress the alloc / free
    // path for parameter default-value AST nodes, then collect.
    // The source string is measured with strlen rather than a
    // hardcoded byte count so the test can't accidentally read
    // past the terminator.
    {
        const char *src =
            "fun f(a, b = a * 2) { return a + b }\n"
            "f(3)";
        int len = (int)strlen(src);
        for (int i = 0; i < 50; i++)
        {
            art_run_source(S, src, len, "<test>");
        }
    }
    art_gc_collect(S);

    expect(S, "defaults still work after GC churn",
           "fun f(a, b = a * 2) { return a + b }\n"
           "f(3)",
           "9");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
