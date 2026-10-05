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
        fprintf(stderr, "FAIL: %s (err)\n", label);
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
        fprintf(stderr, "FAIL: %s -- got '%s', want '%s'\n", label, got, want);
    }
}

int main(void)
{
    printf("ART import tests\n");
    printf("================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    expect(S, "import returns module value",
           "local M = import \"tests/fixtures/mod_simple.art\"\n"
           "M.value",
           "42");

    expect(S, "import cache: second call same object",
           "local A = import \"tests/fixtures/mod_count.art\"\n"
           "local B = import \"tests/fixtures/mod_count.art\"\n"
           "A == B",
           "true");

    expect(S, "import side effects",
           "local C = import \"tests/fixtures/mod_side.art\"\n"
           "the_count",
           "1");

    expect(S, "no extension required",
           "local M = import \"tests/fixtures/mod_simple\"\n"
           "M.value",
           "42");

    expect(S, "no-parens call",
           "local M = import \"tests/fixtures/mod_simple.art\"\n"
           "M.value",
           "42");

    expect(S, "nested import",
           "local M = import \"tests/fixtures/mod_nested.art\"\n"
           "M.deep",
           "99");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}