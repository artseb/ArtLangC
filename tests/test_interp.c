#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "interp.h"
#include "features/registry.h"
#include "gc.h"
#include "parser.h"
#include "art.h"

static int checks = 0;
static int failed = 0;

// Runs src and calls tostring() on the result, comparing the
// UTF-8 bytes against want. Errors increment both counters.
static void expect(ArtState *S, const char *label,
				   const char *src, const char *want)
{
	Value v = art_run_source(S, src, (int)strlen(src), "<test>");
	checks++;

	if (S->control != CONTROL_NONE)
	{
		failed++;
		fprintf(stderr, "FAIL: %s — runtime error\n", label);
		return;
	}

	char got[256];

	// Build "tostring(<src>)" and run it — but src may be multi-
	// statement. Simplest: convert here in C, mirroring tostring.
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
		fprintf(stderr, "FAIL: %s — got '%s', want '%s'\n",
				label, got, want);
	}
}

static void run(ArtState *S, const char *src)
{
	art_run_source(S, src, (int)strlen(src), "<test>");
}

int main(void)
{
	printf("ART interpreter tests\n");
	printf("=====================\n\n");

	ArtState *S = art_state_new();
	art_register_builtins(S);

	// --- arithmetic ---
	expect(S, "1 + 2", "1 + 2", "3");
	expect(S, "10 / 3 floor", "10 / 3", "3");
	expect(S, "10.0 / 3", "10.0 / 3", "3.33333");
	expect(S, "5 and 7", "5 and 7", "7");
	expect(S, "nil or 4", "nil or 4", "4");
	expect(S, "1 == 1", "1 == 1", "true");
	expect(S, "true", "true", "true");
	expect(S, "nil literal", "nil", "nil");

	// --- control flow as return values ---
	expect(S, "while sum",
		   "local i = 1\n"
		   "local sum = 0\n"
		   "while (i <= 5) {\n"
		   "    sum = sum + i\n"
		   "    i = i + 1\n"
		   "}\n"
		   "sum",
		   "15");

	expect(S, "for sum",
		   "local sum = 0\n"
		   "for (local i = 1 -> 10) { sum = sum + i }\n"
		   "sum",
		   "55");

	expect(S, "break",
		   "local i = 0\n"
		   "while (true) {\n"
		   "    i = i + 1\n"
		   "    if (i == 3) { break }\n"
		   "}\n"
		   "i",
		   "3");

	// --- functions and closures ---
	expect(S, "fib(10)",
		   "fun fib(n) {\n"
		   "    if (n < 2) { return n }\n"
		   "    return fib(n - 1) + fib(n - 2)\n"
		   "}\n"
		   "fib(10)",
		   "55");

	run(S,
		"fun makeCounter() {\n"
		"    local n = 0\n"
		"    return fun() { n = n + 1\n return n }\n"
		"}\n"
		"counter = makeCounter()\n");

	expect(S, "counter() #1", "counter()", "1");
	expect(S, "counter() #2", "counter()", "2");
	expect(S, "counter() #3", "counter()", "3");

	// --- tostring builtin ---
	expect(S, "tostring(42)", "tostring(42)", "42");
	expect(S, "tostring(3.5)", "tostring(3.5)", "3.5");
	expect(S, "tostring(true)", "tostring(true)", "true");
	expect(S, "tostring(nil)", "tostring(nil)", "nil");
	expect(S, "tostring(\"hi\")", "tostring(\"hi\")", "hi");

	// --- tables ---
	expect(S, "array index 1", "[1, 2, 3][1]", "1");
	expect(S, "array index 3", "[1, 2, 3][3]", "3");
	expect(S, "array index oob", "[1, 2, 3][4]", "nil");
	expect(S, "hash index", "[\"x\" = 10][\"x\"]", "10");
	expect(S, "member access", "[\"x\" = 10].x", "10");
	expect(S, "missing hash key", "[\"x\" = 10][\"y\"]", "nil");

	expect(S, "index assign in-place",
		   "local t = [1, 2, 3]\n"
		   "t[2] = 99\n"
		   "t[2]",
		   "99");

	expect(S, "index assign append",
		   "local t = []\n"
		   "t[3] = 7\n"
		   "t[3]",
		   "7");

	expect(S, "index assign gap is nil",
		   "local t = []\n"
		   "t[3] = 7\n"
		   "t[1]",
		   "nil");

	expect(S, "string-key assign via index",
		   "local t = []\n"
		   "t[\"k\"] = 5\n"
		   "t.k",
		   "5");

	expect(S, "member assign via dot",
		   "local t = [\"x\" = 10]\n"
		   "t.x = 20\n"
		   "t.x",
		   "20");

	expect(S, "compound index assign",
		   "local t = [10]\n"
		   "t[1] += 5\n"
		   "t[1]",
		   "15");

	expect(S, "compound member assign",
		   "local t = [\"x\" = 10]\n"
		   "t.x += 5\n"
		   "t.x",
		   "15");

	// --- for-in ---
	expect(S, "for-in sum array",
		   "local sum = 0\n"
		   "for (local v in [1, 2, 3, 4]) { sum = sum + v }\n"
		   "sum",
		   "10");

	expect(S, "for-in with keys",
		   "local sum = 0\n"
		   "for (local k, v in [10, 20, 30]) { sum = sum + k + v }\n"
		   "sum",
		   "66");

	expect(S, "for-in over hash",
		   "local sum = 0\n"
		   "for (local k, v in [\"a\" = 1, \"b\" = 2]) { sum = sum + v }\n"
		   "sum",
		   "3");

	expect(S, "for-in break",
		   "local t = [1, 2, 3, 4, 5]\n"
		   "local last = 0\n"
		   "for (local v in t) {\n"
		   "    if (v == 3) { break }\n"
		   "    last = v\n"
		   "}\n"
		   "last",
		   "2");

	expect(S, "for-in continue",
		   "local sum = 0\n"
		   "for (local v in [1, 2, 3, 4, 5]) {\n"
		   "    if (v == 3) { continue }\n"
		   "    sum = sum + v\n"
		   "}\n"
		   "sum",
		   "12");

	// --- per-iteration loop var scope ---
	expect(S, "closure captures per-iteration loop var",
		   "local fns = []\n"
		   "for (local i = 1 -> 3) {\n"
		   "    fns[i] = fun() { return i }\n"
		   "}\n"
		   "fns[1]() + fns[2]() + fns[3]()",
		   "6"); // 1 + 2 + 3; if shared, would be 12 (4 + 4 + 4)

	// --- for requires 'local' ---
	parser_set_suppress(true);
	Node *bad_for = parse_source(S, "for (i = 1 -> 3) { }", 18, "<test>");
	parser_set_suppress(false);
	checks++;
	if (bad_for != NULL)
	{
		failed++;
		fprintf(stderr, "FAIL: for-without-local should fail to parse\n");
		node_free_tree(&bad_for);
	}
	// --- GC ---
	printf("\n--- GC ---\n");

	// A closure must survive explicit collections.
	art_gc_collect(S);
	art_gc_collect(S);
	expect(S, "closure survives GC #1", "counter()", "4");
	art_gc_collect(S);
	expect(S, "closure survives GC #2", "counter()", "5");

	// Generate garbage inside a block: each iteration creates a
	// fresh closure + scope, all of which become unreachable when
	// the block scope pops.
	size_t before = S->gc.bytes_allocated;
	run(S,
		"for (local i = 1 -> 200) {\n"
		"    local f = fun() { return i }\n"
		"}\n");
	size_t mid = S->gc.bytes_allocated;

	art_gc_collect(S);
	size_t after = S->gc.bytes_allocated;

	printf("gc: before=%zu mid=%zu after=%zu\n", before, mid, after);
	checks++;
	if (after > mid)
	{
		failed++;
		fprintf(stderr, "FAIL: GC did not free anything (mid=%zu after=%zu)\n",
				mid, after);
	}

	expect(S, "state usable after GC", "1 + 2", "3");
	expect(S, "closure still alive", "counter()", "6");

	// --- embed API ---
	checks++;
	if (!art_run_string(S, "1 + 2", "<embed>"))
	{
		failed++;
		fprintf(stderr, "FAIL: art_run_string on good input returned false\n");
	}
	checks++;
	parser_set_suppress(true);
	bool bad_ok = art_run_string(S, "local x =", "<embed>");
	parser_set_suppress(false);
	if (bad_ok)
	{
		failed++;
		fprintf(stderr, "FAIL: art_run_string on bad input returned true\n");
	}

	art_state_free(S);

	printf("\n%d checks, %d failed\n", checks, failed);
	return failed == 0 ? 0 : 1;
}