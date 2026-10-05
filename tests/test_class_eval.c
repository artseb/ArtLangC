#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "state.h"
#include "interp.h"
#include "features/registry.h"
#include "parser.h"

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
	Value v = art_run_source(S, src, (int)strlen(src), "<test>");
	(void)v;
	checks++;
	if (!S->last_error)
	{
		failed++;
		fprintf(stderr, "FAIL: %s -- expected runtime error, got none\n",
				label);
	}
	S->last_error = false;
}

static void run(ArtState *S, const char *src)
{
	art_run_source(S, src, (int)strlen(src), "<test>");
}

int main(void)
{
	printf("ART class eval tests\n");
	printf("====================\n\n");

#ifdef _WIN32
	_mkdir("tests/tmp");
#else
	mkdir("tests/tmp", 0755);
#endif

	ArtState *S = art_state_new();
	art_register_builtins(S);

	expect(S, "instance field read",
		   "class Point {\n"
		   "    x = 0\n"
		   "    y = 0\n"
		   "    fun Point(x, y) {\n"
		   "        this.x = x\n"
		   "        this.y = y\n"
		   "    }\n"
		   "}\n"
		   "Point(3, 4).x",
		   "3");

	expect(S, "instance field other",
		   "class Point {\n"
		   "    x = 0\n"
		   "    y = 0\n"
		   "    fun Point(x, y) {\n"
		   "        this.x = x\n"
		   "        this.y = y\n"
		   "    }\n"
		   "}\n"
		   "Point(3, 4).y",
		   "4");

	expect(S, "field default",
		   "class Box {\n"
		   "    v = 99\n"
		   "}\n"
		   "Box().v",
		   "99");

	expect(S, "method call",
		   "class Point {\n"
		   "    x = 0\n"
		   "    y = 0\n"
		   "    fun Point(x, y) {\n"
		   "        this.x = x\n"
		   "        this.y = y\n"
		   "    }\n"
		   "    fun sum() {\n"
		   "        return this.x + this.y\n"
		   "    }\n"
		   "}\n"
		   "Point(3, 4).sum()",
		   "7");

	expect(S, "bound method",
		   "class C {\n"
		   "    n = 0\n"
		   "    fun C(n) { this.n = n }\n"
		   "    fun getN() { return this.n }\n"
		   "}\n"
		   "local c = C(42)\n"
		   "local f = c.getN\n"
		   "f()",
		   "42");

	expect(S, "constructor assigns",
		   "class C {\n"
		   "    v = 0\n"
		   "    fun C(v) { this.v = v }\n"
		   "}\n"
		   "C(123).v",
		   "123");

	expect(S, "arity overload",
		   "class C {\n"
		   "    x = 0\n"
		   "    fun C() { this.x = 1 }\n"
		   "    fun C(a) { this.x = a }\n"
		   "}\n"
		   "C(7).x",
		   "7");

	expect(S, "type overload int",
		   "class C {\n"
		   "    fun f(a: Int)    { return \"int\" }\n"
		   "    fun f(a: String) { return \"str\" }\n"
		   "}\n"
		   "C().f(1)",
		   "int");

	expect(S, "type overload string",
		   "class C {\n"
		   "    fun f(a: Int)    { return \"int\" }\n"
		   "    fun f(a: String) { return \"str\" }\n"
		   "}\n"
		   "C().f(\"x\")",
		   "str");

	// Bound method re-resolves overload at call time, not at access.
	expect(S, "bound overload string",
		   "class C {\n"
		   "    fun f(a: Int)    { return \"int\" }\n"
		   "    fun f(a: String) { return \"str\" }\n"
		   "}\n"
		   "local c = C()\n"
		   "local g = c.f\n"
		   "g(\"x\")",
		   "str");

	// Inheritance: subclass has parent's methods.
	expect(S, "inherited method",
		   "class A {\n"
		   "    fun who() { return \"A\" }\n"
		   "}\n"
		   "class B extends A {\n"
		   "    fun B() { }\n"
		   "}\n"
		   "B().who()",
		   "A");

	// Override: subclass replaces parent's method.
	expect(S, "override",
		   "class A {\n"
		   "    fun who() { return \"A\" }\n"
		   "}\n"
		   "class B extends A {\n"
		   "    fun B() { }\n"
		   "    fun who() { return \"B\" }\n"
		   "}\n"
		   "B().who()",
		   "B");

	// super() calls the parent's constructor.
	expect(S, "super constructor",
		   "class A {\n"
		   "    x = 0\n"
		   "    fun A(x) { this.x = x }\n"
		   "}\n"
		   "class B extends A {\n"
		   "    y = 0\n"
		   "    fun B(x, y) {\n"
		   "        super(x)\n"
		   "        this.y = y\n"
		   "    }\n"
		   "}\n"
		   "local b = B(10, 20)\n"
		   "b.x + b.y",
		   "30");

	// super.method() overrides and calls parent's version.
	expect(S, "super method",
		   "class A {\n"
		   "    fun val() { return 1 }\n"
		   "}\n"
		   "class B extends A {\n"
		   "    fun B() { }\n"
		   "    fun val() { return super.val() + 10 }\n"
		   "}\n"
		   "B().val()",
		   "11");

	// super.method() with args.
	expect(S, "super method args",
		   "class A {\n"
		   "    fun add(x, y) { return x + y }\n"
		   "}\n"
		   "class B extends A {\n"
		   "    fun B() { }\n"
		   "    fun add(x, y) { return super.add(x, y) * 2 }\n"
		   "}\n"
		   "B().add(3, 4)",
		   "14");

	// Getter runs as a method, returns a computed value.
	expect(S, "getter",
		   "class C {\n"
		   "    x = 0\n"
		   "    fun C(x) { this.x = x }\n"
		   "    get doubled() { return this.x * 2 }\n"
		   "}\n"
		   "C(21).doubled",
		   "42");

	// Getter is not a field: no storage, computed each access.
	expect(S, "getter recomputes",
		   "class C {\n"
		   "    x = 1\n"
		   "    get next() { this.x = this.x + 1\n return this.x }\n"
		   "}\n"
		   "local c = C()\n"
		   "c.next\n"
		   "c.next",
		   "3");

	// Setter intercepts assignment.
	expect(S, "setter",
		   "class C {\n"
		   "    _x = 0\n"
		   "    set x(v) { this._x = v * 2 }\n"
		   "    get x()  { return this._x }\n"
		   "}\n"
		   "local c = C()\n"
		   "c.x = 5\n"
		   "c.x",
		   "10");

	// Arithmetic operator overload.
	expect(S, "operator +",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator +(o) { return V(this.x + o.x) }\n"
		   "}\n"
		   "(V(3) + V(4)).x",
		   "7");

	expect(S, "operator -",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator -(o) { return V(this.x - o.x) }\n"
		   "}\n"
		   "(V(10) - V(3)).x",
		   "7");

	// Mixed-type operand with untyped param (matches anything).
	expect(S, "operator * scalar",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator *(s) { return V(this.x * s) }\n"
		   "}\n"
		   "(V(5) * 3).x",
		   "15");

	// Operator == defines equality; != derives from it.
	expect(S, "operator == equal",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator ==(o) { return this.x == o.x }\n"
		   "}\n"
		   "V(3) == V(3)",
		   "true");

	expect(S, "operator != derived",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator ==(o) { return this.x == o.x }\n"
		   "}\n"
		   "V(3) != V(4)",
		   "true");

	// --- const fields (frozen after construction) ---

	expect(S, "const field readable",
		   "class C {\n"
		   "    const x = 0\n"
		   "    fun C(x) { this.x = x }\n"
		   "}\n"
		   "C(42).x",
		   "42");

	// parser_set_suppress(true); // suppresses parser errors, not runtime
	expect_error(S, "const assign after ctor",
				 "class C {\n"
				 "    const x = 0\n"
				 "    fun C(x) { this.x = x }\n"
				 "    fun poke() { this.x = 99 }\n"
				 "}\n"
				 "local c = C(1)\n"
				 "c.poke()\n");

	// --- local const ---
	expect(S, "local const reads",
		   "local const x = 5\n"
		   "x",
		   "5");

	// --- statics ---

	expect(S, "static method",
		   "class M {\n"
		   "    static fun one() { return 1 }\n"
		   "}\n"
		   "M.one()",
		   "1");

	expect(S, "static with args",
		   "class M {\n"
		   "    static fun add(a, b) { return a + b }\n"
		   "}\n"
		   "M.add(3, 4)",
		   "7");

	// --- toString via print ---

	// Can't capture stdout in the harness easily, so test the
	// resolved method directly.
	expect(S, "toString resolves",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    fun toString() { return \"V\" }\n"
		   "}\n"
		   "V(1).toString()",
		   "V");

	// --- builtin methods on primitives ---

	expect(S, "string.upper",
		   "\"hello\".upper()",
		   "HELLO");

	expect(S, "string.length",
		   "\"hello\".length()",
		   "5");

	expect(S, "string + concat",
		   "\"foo\" + \"bar\"",
		   "foobar");

	expect(S, "table.push",
		   "local t = [1, 2]\n"
		   "t.push(3)\n"
		   "t[3]",
		   "3");

	expect(S, "table.length",
		   "[1, 2, 3, 4].length()",
		   "4");

	// --- Math ---
	expect(S, "Math.pi", "Math.pi", "3.14159");
	expect(S, "Math.abs", "Math.abs(-5)", "5");
	expect(S, "Math.floor", "Math.floor(3.7)", "3");
	expect(S, "Math.ceil", "Math.ceil(3.2)", "4");
	expect(S, "Math.sqrt", "Math.sqrt(16)", "4");
	expect(S, "Math.pow", "Math.pow(2, 10)", "1024");
	expect(S, "Math.min int", "Math.min(3, 7)", "3");
	expect(S, "Math.max int", "Math.max(3, 7)", "7");
	expect(S, "Math.clamp", "Math.clamp(15, 0, 10)", "10");
	expect(S, "Math.sign -1", "Math.sign(-3)", "-1");
	expect(S, "Math.sign 0", "Math.sign(0)", "0");

	// Deterministic via seed.
	run(S, "Math.seed(42)");
	Value r1 = art_run_source(S, "Math.random()",
							  (int)strlen("Math.random()"), "<test>");
	run(S, "Math.seed(42)");
	Value r2 = art_run_source(S, "Math.random()",
							  (int)strlen("Math.random()"), "<test>");

	checks++;
	if (!IS_FLOAT(r1) || !IS_FLOAT(r2) ||
		AS_FLOAT(r1) != AS_FLOAT(r2))
	{
		failed++;
		fprintf(stderr, "FAIL: Math.seed deterministic\n");
	}

	// Frozen table: assignment fails.
	expect_error(S, "Math frozen",
				 "Math.pi = 4");

	// --- String methods ---
	expect(S, "str.lower", "\"HELLO\".lower()", "hello");
	expect(S, "str.isEmpty t", "\"\".isEmpty()", "true");
	expect(S, "str.isEmpty f", "\"x\".isEmpty()", "false");
	expect(S, "str.charAt 1", "\"hello\".charAt(1)", "h");
	expect(S, "str.charAt 5", "\"hello\".charAt(5)", "o");
	expect(S, "str.charAt oob", "\"hello\".charAt(99)", "nil");
	expect(S, "str.substring", "\"hello\".substring(2, 4)", "el");
	expect(S, "str.substring 0", "\"hello\".substring(1, 1)", "");
	expect(S, "str.trim", "\"  hi  \".trim()", "hi");
	expect(S, "str.trim only", "\"   \".trim()", "");
	expect(S, "str.toInt", "\"42\".toInt()", "42");
	expect(S, "str.toInt neg", "\"  -7 \".toInt()", "-7");
	expect(S, "str.toInt bad", "\"abc\".toInt()", "nil");
	expect(S, "str.toInt trail", "\"42x\".toInt()", "nil");
	expect(S, "str.toFloat", "\"3.14\".toFloat()", "3.14");
	expect(S, "str.toFloat bad", "\"x\".toFloat()", "nil");

	expect(S, "str.startsWith t", "\"hello\".startsWith(\"he\")", "true");
	expect(S, "str.startsWith f", "\"hello\".startsWith(\"lo\")", "false");
	expect(S, "str.endsWith t", "\"hello\".endsWith(\"lo\")", "true");
	expect(S, "str.endsWith f", "\"hello\".endsWith(\"he\")", "false");
	expect(S, "str.contains t", "\"hello\".contains(\"ell\")", "true");
	expect(S, "str.contains f", "\"hello\".contains(\"xyz\")", "false");
	expect(S, "str.indexOf", "\"hello\".indexOf(\"l\")", "3");
	expect(S, "str.indexOf miss", "\"hello\".indexOf(\"z\")", "nil");
	expect(S, "str.indexOf from", "\"hello\".indexOf(\"l\", 4)", "4");
	expect(S, "str.reverse", "\"abc\".reverse()", "cba");

	expect(S, "str.split basic",
		   "\"a-b-c\".split(\"-\")[1]",
		   "a");
	expect(S, "str.split count",
		   "\"a-b-c\".split(\"-\").length()",
		   "3");
	expect(S, "str.split empty sep",
		   "\"abc\".split(\"\").length()",
		   "3");
	expect(S, "str.split no match",
		   "\"abc\".split(\"-\")[1]",
		   "abc");

	// --- Table methods ---
	expect(S, "t.pop", "[1, 2, 3].pop()", "3");
	expect(S, "t.isEmpty t", "[].isEmpty()", "true");
	expect(S, "t.isEmpty f", "[1].isEmpty()", "false");
	expect(S, "t.contains t", "[1, 2, 3].contains(2)", "true");
	expect(S, "t.contains f", "[1, 2, 3].contains(9)", "false");
	expect(S, "t.indexOf", "[10, 20, 30].indexOf(20)", "2");
	expect(S, "t.indexOf miss", "[10, 20].indexOf(99)", "nil");

	expect(S, "t.reverse",
		   "local t = [1, 2, 3]\n"
		   "t.reverse()\n"
		   "t[1] * 100 + t[2] * 10 + t[3]",
		   "321");

	expect(S, "t.clone independent",
		   "local a = [1, 2, 3]\n"
		   "local b = a.clone()\n"
		   "b[1] = 99\n"
		   "a[1]",
		   "1");

	expect(S, "t.keys length",
		   "[\"a\" = 1, \"b\" = 2].keys().length()",
		   "2");

	expect(S, "t.values length",
		   "[10, 20, 30].values().length()",
		   "3");

	expect(S, "t.clear",
		   "local t = [1, 2, 3]\n"
		   "t.clear()\n"
		   "t.length()",
		   "0");

	expect(S, "t.insert",
		   "local t = [1, 3]\n"
		   "t.insert(2, 2)\n"
		   "t[2]",
		   "2");

	expect(S, "t.remove",
		   "local t = [1, 2, 3]\n"
		   "t.remove(2)",
		   "2");

	expect(S, "t.remove shifts",
		   "local t = [1, 2, 3]\n"
		   "t.remove(1)\n"
		   "t[1]",
		   "2");

	// --- toString dispatch ---

	expect(S, "tostring instance uses toString",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    fun toString() { return \"V(\" + tostring(this.x) + \")\" }\n"
		   "}\n"
		   "tostring(V(7))",
		   "V(7)");

	expect(S, "tostring no toString",
		   "class Bare {\n"
		   "    fun Bare() { }\n"
		   "}\n"
		   "tostring(Bare())",
		   "<Bare instance>");

	expect(S, "tostring str concat with toString",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    fun toString() { return \"<\" + tostring(this.x) + \">\" }\n"
		   "}\n"
		   "\"got \" + tostring(V(5))",
		   "got <5>");

	// --- enum ---

	expect(S, "enum tostring",
		   "enum Color { Red, Green, Blue }\n"
		   "tostring(Color.Red)",
		   "Color.Red");

	expect(S, "enum name",
		   "enum Color { Red, Green }\n"
		   "Color.Green.name",
		   "Green");

	expect(S, "enum value assigned",
		   "enum Http { OK = 200, NotFound = 404 }\n"
		   "Http.NotFound.value",
		   "404");

	expect(S, "enum value unassigned nil",
		   "enum Color { Red, Green }\n"
		   "Color.Red.value",
		   "nil");

	expect(S, "enum fromName",
		   "enum Color { Red, Green }\n"
		   "tostring(Color.fromName(\"Green\"))",
		   "Color.Green");

	expect(S, "enum fromName miss",
		   "enum Color { Red }\n"
		   "Color.fromName(\"Purple\")",
		   "nil");

	expect(S, "enum fromValue",
		   "enum Http { OK = 200, NotFound = 404 }\n"
		   "tostring(Http.fromValue(404))",
		   "Http.NotFound");

	expect(S, "enum values length",
		   "enum Color { Red, Green, Blue }\n"
		   "Color.values().length()",
		   "3");

	expect(S, "enum names ordered",
		   "enum Color { Red, Green, Blue }\n"
		   "Color.names()[2]",
		   "Green");

	expect(S, "enum equality",
		   "enum Color { Red, Green }\n"
		   "Color.Red == Color.Red",
		   "true");

	expect(S, "enum distinct types",
		   "enum A { Red }\n"
		   "enum B { Red }\n"
		   "A.Red == B.Red",
		   "false");

	expect_error(S, "enum mixed types",
				 "enum Mixed { A = 1, B = \"two\" }");

	expect_error(S, "enum assign frozen",
				 "enum Color { Red }\n"
				 "Color.Red = 5");

	// --- switch ---

	expect(S, "switch int arm 1",
		   "local x = 1\n"
		   "switch (x) {\n"
		   "    1 -> \"one\"\n"
		   "    2 -> \"two\"\n"
		   "    else -> \"other\"\n"
		   "}",
		   "one");

	expect(S, "switch int arm 2",
		   "local x = 2\n"
		   "switch (x) {\n"
		   "    1 -> \"one\"\n"
		   "    2 -> \"two\"\n"
		   "    else -> \"other\"\n"
		   "}",
		   "two");

	expect(S, "switch else",
		   "local x = 99\n"
		   "switch (x) {\n"
		   "    1 -> \"one\"\n"
		   "    else -> \"other\"\n"
		   "}",
		   "other");

	expect(S, "switch no else",
		   "local x = 99\n"
		   "switch (x) {\n"
		   "    1 -> \"one\"\n"
		   "}",
		   "nil");

	expect(S, "switch multi-value arm",
		   "local x = 20\n"
		   "switch (x) {\n"
		   "    1, 2 -> \"low\"\n"
		   "    10, 20, 30 -> \"ten\"\n"
		   "    else -> \"other\"\n"
		   "}",
		   "ten");

	expect(S, "switch string",
		   "local s = \"bob\"\n"
		   "switch (s) {\n"
		   "    \"alice\" -> 1\n"
		   "    \"bob\" -> 2\n"
		   "    else -> 0\n"
		   "}",
		   "2");

	expect(S, "switch bool",
		   "local b = true\n"
		   "switch (b) {\n"
		   "    true -> \"yes\"\n"
		   "    false -> \"no\"\n"
		   "}",
		   "yes");

	expect(S, "switch block body",
		   "local x = 5\n"
		   "switch (x) {\n"
		   "    5 -> { local y = 10\n y * 2 }\n"
		   "    else -> 0\n"
		   "}",
		   "20");

	expect(S, "switch enum",
		   "enum Color { Red, Green, Blue }\n"
		   "local c = Color.Green\n"
		   "switch (c) {\n"
		   "    Color.Red -> \"r\"\n"
		   "    Color.Green, Color.Blue -> \"cool\"\n"
		   "    else -> \"?\"\n"
		   "}",
		   "cool");

	expect(S, "switch statement form",
		   "local x = 2\n"
		   "local msg = \"\"\n"
		   "switch (x) {\n"
		   "    1 -> { msg = \"one\" }\n"
		   "    2 -> { msg = \"two\" }\n"
		   "}\n"
		   "msg",
		   "two");

	expect_error(S, "switch type mismatch",
				 "local x = 99\n"
				 "switch (x) {\n"
				 "    1 -> \"a\"\n"
				 "    \"b\" -> \"b\"\n"
				 "    else -> \"other\"\n"
				 "}");

	// --- File ---

	expect(S, "File.write then read",
		   "File.write(\"tests/tmp/f.txt\", \"hello\\nworld\\n\")\n"
		   "File.read(\"tests/tmp/f.txt\")",
		   "hello\nworld\n");

	expect(S, "File.read missing -> nil",
		   "File.read(\"tests/tmp/does_not_exist.txt\")",
		   "nil");

	expect(S, "File.append",
		   "File.write(\"tests/tmp/f2.txt\", \"a\")\n"
		   "File.append(\"tests/tmp/f2.txt\", \"b\")\n"
		   "File.read(\"tests/tmp/f2.txt\")",
		   "ab");

	expect(S, "File instance readLine",
		   "File.write(\"tests/tmp/lines.txt\", \"one\\ntwo\\nthree\\n\")\n"
		   "local f = File.open(\"tests/tmp/lines.txt\")\n"
		   "local a = f.readLine()\n"
		   "f.close()\n"
		   "a",
		   "one");

	expect(S, "File instance readLines length",
		   "File.write(\"tests/tmp/lines2.txt\", \"a\\nb\\nc\\n\")\n"
		   "local f = File.open(\"tests/tmp/lines2.txt\")\n"
		   "local ls = f.readLines()\n"
		   "f.close()\n"
		   "ls.length()",
		   "3");

	expect(S, "File instance readLine at EOF",
		   "File.write(\"tests/tmp/empty.txt\", \"\")\n"
		   "local f = File.open(\"tests/tmp/empty.txt\")\n"
		   "local l = f.readLine()\n"
		   "f.close()\n"
		   "l",
		   "nil");

	expect(S, "File.delete",
		   "File.write(\"tests/tmp/del.txt\", \"x\")\n"
		   "File.delete(\"tests/tmp/del.txt\")\n"
		   "File.read(\"tests/tmp/del.txt\")",
		   "nil");

	// --- variadics ---

	expect(S, "variadic sum",
		   "fun sum(...args) {\n"
		   "    local t = 0\n"
		   "    for (v in args) { t = t + v }\n"
		   "    return t\n"
		   "}\n"
		   "sum(1, 2, 3, 4)",
		   "10");

	expect(S, "variadic empty rest",
		   "fun count(...args) { return args.length() }\n"
		   "count()",
		   "0");

	expect(S, "variadic with fixed",
		   "fun f(a, ...rest) { return a + rest.length() }\n"
		   "f(10, 1, 2, 3)",
		   "13");

	// --- defaults ---

	expect(S, "default used",
		   "fun f(a, b = 5) { return a + b }\n"
		   "f(1)",
		   "6");

	expect(S, "default overridden",
		   "fun f(a, b = 5) { return a + b }\n"
		   "f(1, 10)",
		   "11");

	expect(S, "all defaults",
		   "fun f(a = 1, b = 2, c = 3) { return a + b + c }\n"
		   "f()",
		   "6");

	expect(S, "default references earlier param",
		   "fun f(a, b = a * 2) { return a + b }\n"
		   "f(5)",
		   "15");

	expect(S, "defaults + variadic",
		   "fun f(a, b = 10, ...rest) { return a + b + rest.length() }\n"
		   "f(1, 2, 3, 4, 5)",
		   "6"); // 1 + 2 + 3

	// --- method form ---

	expect(S, "method default",
		   "class C {\n"
		   "    fun m(a, b = 100) { return a + b }\n"
		   "}\n"
		   "C().m(1)",
		   "101");

	expect(S, "method variadic",
		   "class C {\n"
		   "    fun m(a, ...rest) { return a * rest.length() }\n"
		   "}\n"
		   "C().m(3, 1, 2)",
		   "6");

	// --- overload resolution respects defaults ---

	expect(S, "overload picks by arity with defaults",
		   "class C {\n"
		   "    fun f() { return \"zero\" }\n"
		   "    fun f(a) { return \"one\" }\n"
		   "    fun f(a, b) { return \"two\" }\n"
		   "}\n"
		   "C().f() + \",\" + C().f(1) + \",\" + C().f(1, 2)",
		   "zero,one,two");

	// --- string interpolation ---

	expect(S, "interp simple",
		   "local name = \"world\"\n"
		   "\"hello ${name}!\"",
		   "hello world!");

	expect(S, "interp int",
		   "local x = 5\n"
		   "\"x = ${x}, double = ${x * 2}\"",
		   "x = 5, double = 10");

	expect(S, "interp multiple",
		   "local a = 1\n"
		   "local b = 2\n"
		   "\"${a} + ${b} = ${a + b}\"",
		   "1 + 2 = 3");

	expect(S, "interp no expression",
		   "\"just text\"",
		   "just text");

	expect(S, "interp empty expression result",
		   "\"pre${nil}post\"",
		   "prenilpost");

	expect(S, "interp escaped dollar",
		   "\"\\${not_interp}\"",
		   "${not_interp}");

	expect(S, "interp string in expr",
		   "local n = \"bob\"\n"
		   "\"hi ${n.upper()}\"",
		   "hi BOB");

	expect(S, "interp inside table literal",
		   "local x = 42\n"
		   "local t = [\"key\" = \"v = ${x}\"]\n"
		   "t[\"key\"]",
		   "v = 42");

	expect(S, "interp in method",
		   "class C {\n"
		   "    n = 7\n"
		   "    fun C() { }\n"
		   "    fun show() { return \"n=${this.n}\" }\n"
		   "}\n"
		   "C().show()",
		   "n=7");

	// --- destructuring ---

	expect(S, "destructure basic",
		   "local a, b, c = [1, 2, 3]\n"
		   "a * 100 + b * 10 + c",
		   "123");

	expect(S, "destructure missing slots",
		   "local a, b, c = [1]\n"
		   "b == nil and c == nil",
		   "true");

	expect(S, "destructure extra slots ignored",
		   "local a, b = [1, 2, 3, 4]\n"
		   "a + b",
		   "3");

	expect(S, "destructure attempt",
		   "local ok, v = attempt(fun() { return 42 })\n"
		   "if (ok) { return v }\n"
		   "return -1",
		   "42");

	// --- attempt ---

	expect(S, "attempt success bool",
		   "local ok, v = attempt(fun() { return 5 })\n"
		   "ok",
		   "true");

	expect(S, "attempt failure bool",
		   "local ok, v = attempt(fun() { return nil + 1 })\n"
		   "ok",
		   "false");

	expect(S, "attempt failure message present",
		   "local ok, v = attempt(fun() { return nil + 1 })\n"
		   "v.contains(\"arithmetic\")",
		   "true");

	expect(S, "attempt passes args",
		   "fun add(a, b) { return a + b }\n"
		   "local ok, v = attempt(add, 3, 4)\n"
		   "v",
		   "7");

	expect(S, "attempt with builtin",
		   "local ok, v = attempt(Math.sqrt, 16)\n"
		   "v",
		   "4");

	expect(S, "attempt nested returns outer value",
		   "local r = attempt(fun() {\n"
		   "    local inner = attempt(fun() { return 1 / 0 })\n"
		   "    return \"outer\"\n"
		   "})\n"
		   "r[2]",
		   "outer");

	expect(S, "attempt state recovery",
		   "local ok = attempt(fun() { return nil + 1 })[1]\n"
		   "1 + 1",
		   "2");

		       expect(S, "t.map",
        "local out = [1, 2, 3].map(fun(x) { return x * 2 })\n"
        "out[1] + out[2] + out[3]",
        "12");

    expect(S, "t.filter",
        "local out = [1, 2, 3, 4, 5].filter(fun(x) { return x % 2 == 0 })\n"
        "out.length()",
        "2");

    expect(S, "t.filter values",
        "local out = [1, 2, 3, 4, 5].filter(fun(x) { return x % 2 == 0 })\n"
        "out[1] + out[2]",
        "6");

    expect(S, "t.reduce",
        "[1, 2, 3, 4].reduce(fun(acc, x) { return acc + x }, 0)",
        "10");

    expect(S, "t.reduce with strings",
        "[\"a\", \"b\", \"c\"].reduce(fun(acc, x) { return acc + x }, \"\")",
        "abc");

    expect(S, "t.any true",
        "[1, 2, 3].any(fun(x) { return x > 2 })",
        "true");

    expect(S, "t.any false",
        "[1, 2, 3].any(fun(x) { return x > 100 })",
        "false");

    expect(S, "t.all true",
        "[2, 4, 6].all(fun(x) { return x % 2 == 0 })",
        "true");

    expect(S, "t.all false",
        "[2, 3, 6].all(fun(x) { return x % 2 == 0 })",
        "false");

    expect(S, "t.find hit",
        "[1, 2, 3, 4].find(fun(x) { return x > 2 })",
        "3");

    expect(S, "t.find miss",
        "[1, 2].find(fun(x) { return x > 100 })",
        "nil");

    expect(S, "t.count",
        "[1, 2, 3, 4, 5].count(fun(x) { return x > 2 })",
        "3");

    expect(S, "t.sort numbers",
        "local t = [3, 1, 2]\n"
        "t.sort()\n"
        "t[1] * 100 + t[2] * 10 + t[3]",
        "123");

    expect(S, "t.sort strings",
        "local t = [\"cherry\", \"apple\", \"banana\"]\n"
        "t.sort()\n"
        "t[1]",
        "apple");

    expect(S, "t.sort with comparator desc",
        "local t = [3, 1, 2]\n"
        "t.sort(fun(a, b) { return a > b })\n"
        "t[1] * 100 + t[2] * 10 + t[3]",
        "321");

    expect(S, "t.forEach sum via closure",
        "local sum = 0\n"
        "[1, 2, 3, 4].forEach(fun(x) { sum = sum + x })\n"
        "sum",
        "10");

	// --- Time ---

	expect(S, "Time.now is a number",
		   "Time.now() > 0",
		   "true");

	expect(S, "Time.clock is monotonic",
		   "local a = Time.clock()\n"
		   "local b = Time.clock()\n"
		   "b >= a",
		   "true");

	expect(S, "Time.sleep returns nil",
		   "Time.sleep(1)",
		   "nil");

	// --- error ---

	expect(S, "error caught by attempt",
		   "local ok, msg = attempt(fun() { error(\"boom\") })\n"
		   "ok",
		   "false");

	expect(S, "error message preserved",
		   "local ok, msg = attempt(fun() { error(\"user not found\") })\n"
		   "msg.contains(\"user not found\")",
		   "true");

	// --- is operator ---

	expect(S, "is Int", "5 is Int", "true");
	expect(S, "is Int on float", "5.0 is Int", "false");
	expect(S, "is Number int", "5 is Number", "true");
	expect(S, "is Number float", "5.5 is Number", "true");
	expect(S, "is String", "\"hi\" is String", "true");
	expect(S, "is Bool", "true is Bool", "true");
	expect(S, "is Nil", "nil is Nil", "true");
	expect(S, "is Table", "[1, 2] is Table", "true");
	expect(S, "is Function", "(fun() {}) is Function", "true");
	expect(S, "is Any", "5 is Any", "true");

	expect(S, "is instance",
		   "class V { fun V() { } }\n"
		   "V() is V",
		   "true");

	expect(S, "is instance wrong class",
		   "class V { fun V() { } }\n"
		   "class W { fun W() { } }\n"
		   "V() is W",
		   "false");

	expect(S, "is subclass",
		   "class A { fun A() { } }\n"
		   "class B extends A { fun B() { } }\n"
		   "B() is A",
		   "true");

	expect_error(S, "is unknown type", "5 is NotAThing");

	// --- in operator ---

	expect(S, "in array value", "2 in [1, 2, 3]", "true");
	expect(S, "in array miss", "5 in [1, 2, 3]", "false");
	expect(S, "in hash key", "\"x\" in [\"x\" = 1, \"y\" = 2]", "true");
	expect(S, "in hash key miss", "\"z\" in [\"x\" = 1]", "false");
	expect(S, "in hash value no key", "1 in [\"x\" = 1]", "false");
	expect(S, "in string", "\"wor\" in \"hello world\"", "true");
	expect(S, "in string miss", "\"xyz\" in \"hello\"", "false");

	expect(S, "in enum",
		   "enum C { R, G, B }\n"
		   "C.G in C",
		   "true");

	expect(S, "in enum miss",
		   "enum C { R, G }\n"
		   "enum D { X, Y }\n"
		   "D.X in C",
		   "false");

	// --- symmetric operators ---

	expect(S, "scalar * vector",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator *(s: Number) { return V(this.x * s) }\n"
		   "}\n"
		   "(2 * V(5)).x",
		   "10");

	expect(S, "scalar + vector",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator +(s: Number) { return V(this.x + s) }\n"
		   "}\n"
		   "(10 + V(5)).x",
		   "15");

	expect(S, "vector == scalar reverse",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator ==(o) { if (o is V) { return this.x == o.x }\n"
		   "                     return this.x == o }\n"
		   "}\n"
		   "5 == V(5)",
		   "true");

	expect(S, "non-commutative no reverse",
		   "class V {\n"
		   "    x = 0\n"
		   "    fun V(x) { this.x = x }\n"
		   "    operator -(s: Number) { return V(this.x - s) }\n"
		   "}\n"
		   "local ok = attempt(fun() { return 10 - V(5) })[1]\n"
		   "ok",
		   "false");

	// --- interfaces ---

	expect(S, "interface basic",
		   "interface D {\n"
		   "    fun draw()\n"
		   "}\n"
		   "class Circle implements D {\n"
		   "    fun Circle() { }\n"
		   "    fun draw() { return \"circle\" }\n"
		   "}\n"
		   "Circle().draw()",
		   "circle");

	expect_error(S, "interface missing method",
				 "interface D {\n"
				 "    fun draw()\n"
				 "    fun bounds()\n"
				 "}\n"
				 "class Circle implements D {\n"
				 "    fun Circle() { }\n"
				 "    fun draw() { return \"circle\" }\n"
				 "}\n");

	expect(S, "is interface true",
		   "interface D { fun d() }\n"
		   "class C implements D {\n"
		   "    fun C() { }\n"
		   "    fun d() { }\n"
		   "}\n"
		   "C() is D",
		   "true");

	expect(S, "is interface false",
		   "interface D { fun d() }\n"
		   "class C { fun C() { } }\n"
		   "C() is D",
		   "false");

	expect(S, "interface inheritance",
		   "interface Animal { fun breathe() }\n"
		   "interface Pet extends Animal { fun name() }\n"
		   "class Dog implements Pet {\n"
		   "    fun Dog() { }\n"
		   "    fun breathe() { }\n"
		   "    fun name() { return \"rex\" }\n"
		   "}\n"
		   "Dog() is Animal",
		   "true");

	expect_error(S, "interface arity mismatch",
				 "interface D { fun draw() }\n"
				 "class C implements D {\n"
				 "    fun C() { }\n"
				 "    fun draw(x) { }\n"
				 "}\n");

	expect(S, "interface arity satisfied by defaults",
		   "interface D { fun draw() }\n"
		   "class C implements D {\n"
		   "    fun C() { }\n"
		   "    fun draw(x = 0) { return \"ok\" }\n"
		   "}\n"
		   "C().draw()",
		   "ok");

	expect(S, "interface with getter",
		   "interface Named {\n"
		   "    get name\n"
		   "}\n"
		   "class P implements Named {\n"
		   "    name = \"bob\"\n"
		   "    fun P() { }\n"
		   "}\n"
		   "P() is Named",
		   "true");

	expect_error(S, "interface getter missing",
				 "interface Named { get name }\n"
				 "class P implements Named {\n"
				 "    fun P() { }\n"
				 "}\n");

	expect(S, "interface method via superclass",
		   "interface D { fun draw() }\n"
		   "class Base { fun draw() { return \"base\" } }\n"
		   "class Sub extends Base implements D {\n"
		   "    fun Sub() { }\n"
		   "}\n"
		   "Sub().draw()",
		   "base");

	expect(S, "interface type annotation",
		   "interface Named { get name }\n"
		   "class P implements Named {\n"
		   "    name = \"alice\"\n"
		   "    fun P() { }\n"
		   "}\n"
		   "fun greet(n: Named) { return \"hi \" + n.name }\n"
		   "greet(P())",
		   "hi alice");

	expect_error(S, "interface multiple, one missing",
				 "interface A { fun a() }\n"
				 "interface B { fun b() }\n"
				 "class C implements A, B {\n"
				 "    fun C() { }\n"
				 "    fun a() { }\n"
				 "}\n");

	expect(S, "interface multiple satisfied",
		   "interface A { fun a() }\n"
		   "interface B { fun b() }\n"
		   "class C implements A, B {\n"
		   "    fun C() { }\n"
		   "    fun a() { return 1 }\n"
		   "    fun b() { return 2 }\n"
		   "}\n"
		   "C().a() + C().b()",
		   "3");

	art_state_free(S);

	printf("\n%d checks, %d failed\n", checks, failed);
	return failed == 0 ? 0 : 1;
}