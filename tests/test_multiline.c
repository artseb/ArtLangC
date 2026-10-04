// ============================================================
// test_multiline.c — multiline expressions, nested strings in
// interpolation, and static class fields.
//
// Two of these would have caught bugs found while writing
// examples/collections.art:
//   - `${names.join(", ")}` — a nested string inside an
//     interpolation was terminating the outer string at the
//     lexer level. Fixed in lexer.c::scan_string.
//   - `static made = 0` + `Item.made = ...` — static fields
//     were parsed but never reached at runtime. Fixed in
//     class_decl.c and class_feature.c.
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

int main(void)
{
       printf("ART multiline + collections tests\n");
       printf("=================================\n\n");

       ArtState *S = art_state_new();
       art_register_builtins(S);

       // -------- multiline expressions --------

       expect(S, "plus on next line",
              "local x = 1\n"
              "+ 2\n"
              "x",
              "3");

       expect(S, "chained methods",
              "local t = [1, 2, 3]\n"
              "local s = t\n"
              "    .map(fun(n) { return n * 2 })\n"
              "    .reduce(fun(acc, n) { return acc + n }, 0)\n"
              "s",
              "12");

       expect(S, "warmup-style reduce split",
              "local t = [1, 2, 3, 4]\n"
              "local avg = t.reduce(fun(acc, n) { return acc + n }, 0.0)\n"
              "                / t.length()\n"
              "avg",
              "2.5");

       expect(S, "newline ends assignment",
              "local x = 1\n"
              "local y = 2\n"
              "x + y",
              "3");

       // -------- nested strings in interpolation --------

       expect(S, "nested string in interpolation",
              "local names = [\"a\", \"b\", \"c\"]\n"
              "\"joined: ${names.join(\", \")}\"",
              "joined: a, b, c");

       expect(S, "nested string at start of interp",
              "local t = [\"x\"]\n"
              "\"${t.join(\"-\")}!\"",
              "x!");

       expect(S, "nested string with braces inside",
              "local s = \"a{b\"\n"
              "\"pre ${s} post\"",
              "pre a{b post");

       // -------- slice and join --------

       expect(S, "slice basic",
              "[10, 20, 30, 40].slice(2, 3).length()",
              "2");

       expect(S, "slice values",
              "local t = [10, 20, 30, 40].slice(2, 3)\n"
              "t[1] * 100 + t[2]",
              "2030");

       expect(S, "slice clamps",
              "[10, 20].slice(1, 99).length()",
              "2");

       expect(S, "join numbers",
              "[1, 2, 3].join(\", \")",
              "1, 2, 3");

       expect(S, "join single element",
              "[42].join(\"-\")",
              "42");

       // -------- static class fields --------

       expect(S, "static field read",
              "class M { static made = 0 }\n"
              "M.made",
              "0");

       expect(S, "static field write",
              "class M { static made = 0 }\n"
              "M.made = 5\n"
              "M.made",
              "5");

       expect(S, "static field from constructor",
              "class Item {\n"
              "    static made = 0\n"
              "    fun Item() { Item.made = Item.made + 1 }\n"
              "}\n"
              "Item()\n"
              "Item()\n"
              "Item()\n"
              "Item.made",
              "3");

       // -------- the shape from examples/collections.art --------
       //
       // `name` is a public field here — the test reads it from a
       // lambda outside the class, so it can't be `local`. Same for
       // `price`. Marking a field `local` makes it private to the
       // class body, and this shape intentionally exposes both.

       expect(S, "collections.art shape",
              "class Item {\n"
              "    static made = 0\n"
              "    name\n"
              "    price\n"
              "    fun Item(name, price) {\n"
              "        this.name = name\n"
              "        this.price = price\n"
              "        Item.made = Item.made + 1\n"
              "    }\n"
              "    fun toString() { return this.name }\n"
              "}\n"
              "local shop = [Item(\"Sword\", 100), Item(\"Rope\", 5)]\n"
              "local names = shop.map(fun(item) { return item.name })\n"
              "\"stock: ${shop.length()} of ${Item.made} ever; names: ${names.join(\", \")}\"",
              "stock: 2 of 2 ever; names: Sword, Rope");

       // A private field can be read from inside the class, but
       // not from outside. The lambda here runs at top level, so
       // the read fails. This is the rule the test above was
       // accidentally exercising.
       {
              const char *src =
                  "class Item {\n"
                  "    local secret\n"
                  "    fun Item() { this.secret = 42 }\n"
                  "}\n"
                  "Item().secret";
              art_run_source(S, src, (int)strlen(src), "<test>");
              checks++;
              if (!S->last_error)
              {
                     failed++;
                     fprintf(stderr,
                             "FAIL: private field read from outside should error\n");
              }
              S->last_error = false;
       }

       art_state_free(S);

       printf("\n%d checks, %d failed\n", checks, failed);
       return failed == 0 ? 0 : 1;
}
