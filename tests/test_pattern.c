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

int main(void)
{
    printf("ART pattern tests\n");
    printf("=================\n\n");

    ArtState *S = art_state_new();
    art_register_builtins(S);

    // ============================================================
    // find
    // ============================================================

    expect(S, "find literal start",
           "\"hello world\".find(\"world\")[1]",
           "7");

    expect(S, "find literal end",
           "\"hello world\".find(\"world\")[2]",
           "11");

    expect(S, "find miss",
           "\"hello\".find(\"xyz\")",
           "nil");

    expect(S, "find with class",
           "\"abc123\".find(\"%d+\")[1]",
           "4");

    expect(S, "find with init",
           "\"abcabc\".find(\"a\", 2)[1]",
           "4");

    expect(S, "find with captures",
           "\"key=value\".find(\"(%a+)=(%a+)\")[3]",
           "key");

    // ============================================================
    // match
    // ============================================================

    expect(S, "match whole",
           "\"hello world\".match(\"world\")",
           "world");

    expect(S, "match with capture",
           "\"key=value\".match(\"(%a+)=(%a+)\")[1]",
           "key");

    expect(S, "match capture 2",
           "\"key=value\".match(\"(%a+)=(%a+)\")[2]",
           "value");

    expect(S, "match single capture returns string",
           "\"key=value\".match(\"(%a+)=%a+\")",
           "key");

    expect(S, "match digits",
           "\"abc123\".match(\"%d+\")",
           "123");

    expect(S, "match miss",
           "\"abc\".match(\"%d+\")",
           "nil");

    // ============================================================
    // classes
    // ============================================================

    expect(S, "class %a", "\"abc\".match(\"%a+\")", "abc");
    expect(S, "class %d", "\"123\".match(\"%d+\")", "123");
    expect(S, "class %w", "\"ab12\".match(\"%w+\")", "ab12");
    expect(S, "class %s", "\"  hi\".match(\"%s+\")", "  ");
    expect(S, "class %l", "\"aBcD\".match(\"%l+\")", "a");
    expect(S, "class %u", "\"aBcD\".match(\"%u+\")", "B");
    expect(S, "class %p", "\"hi!\".match(\"%p\")", "!");
    expect(S, "class %x", "\"xyz0129ff\".match(\"%x+\")", "0129ff");
    expect(S, "class complement %A", "\"123abc\".match(\"%A+\")", "123");

    // ============================================================
    // sets and ranges
    // ============================================================

    expect(S, "set basic",
           "\"hello\".match(\"[aeiou]+\")",
           "e");

    expect(S, "set range",
           "\"abc123\".match(\"[a-z]+\")",
           "abc");

    expect(S, "set negation",
           "\"abc123\".match(\"[^a-z]+\")",
           "123");

    expect(S, "set with class and space",
           "\"abc 123\".match(\"[%d ]+\")",
           " 123");

    // ============================================================
    // quantifiers
    // ============================================================

    expect(S, "greedy star",
           "\"aaab\".match(\"a*\")",
           "aaa");

    expect(S, "plus",
           "\"aaab\".match(\"a+\")",
           "aaa");

    expect(S, "lazy minus",
           "\"<a><b>\".match(\"<.->\")",
           "<a>");

    expect(S, "optional present",
           "\"color\".match(\"colou?r\")",
           "color");

    expect(S, "optional absent",
           "\"color\".match(\"colour\")",
           "nil");

    // ============================================================
    // anchors
    // ============================================================

    expect(S, "anchor start",
           "\"hello\".match(\"^h\")",
           "h");

    expect(S, "anchor start miss",
           "\"ahello\".match(\"^h\")",
           "nil");

    expect(S, "anchor end",
           "\"hello\".match(\"o$\")",
           "o");

    expect(S, "anchor end miss",
           "\"helloX\".match(\"o$\")",
           "nil");

    // ============================================================
    // gmatch
    // ============================================================

    expect(S, "gmatch words count",
           "\"foo bar baz\".gmatch(\"%a+\").length()",
           "3");

    expect(S, "gmatch first word",
           "\"foo bar baz\".gmatch(\"%a+\")[1]",
           "foo");

    expect(S, "gmatch last word",
           "\"foo bar baz\".gmatch(\"%a+\")[3]",
           "baz");

    expect(S, "gmatch digits",
           "\"a1b22c333\".gmatch(\"%d+\")[2]",
           "22");

    expect(S, "gmatch with captures",
           "\"a=1,b=2\".gmatch(\"(%a+)=(%d+)\")[2][1]",
           "b");

    // ============================================================
    // gsub
    // ============================================================

    expect(S, "gsub literal",
           "\"hello world\".gsub(\"world\", \"there\")",
           "hello there");

    expect(S, "gsub all occurrences",
           "\"a-b-c\".gsub(\"-\", \"+\")",
           "a+b+c");

    expect(S, "gsub no match",
           "\"hello\".gsub(\"xyz\", \"!\")",
           "hello");

    expect(S, "gsub with capture reference",
           "\"a=1,b=2\".gsub(\"(%a+)=(%d+)\", \"%2:%1\")",
           "1:a,2:b");

    expect(S, "gsub with whole match",
           "\"cat\".gsub(\"a\", \"[%0]\")",
           "c[a]t");

    expect(S, "gsub with literal percent",
           "\"a\".gsub(\"a\", \"%%\")",
           "%");

    expect(S, "gsub digits to hash",
           "\"a1b2c3\".gsub(\"%d\", \"#\")",
           "a#b#c#");

    expect(S, "gsub with function",
           "\"a1b2\".gsub(\"%d\", fun(d) { return \"<\" + d + \">\" })",
           "a<1>b<2>");

    // ============================================================
    // real-world shapes
    // ============================================================

    expect(S, "parse key=value",
           "\"host=localhost port=8080\".gmatch(\"(%a+)=(%a+)\")[1][2]",
           "localhost");

    expect(S, "strip html-ish tags",
           "\"<b>hello</b>\".gsub(\"<[^>]*>\", \"\")",
           "hello");

    expect(S, "extract all numbers",
           "\"x1 y22 z333\".gmatch(\"%d+\").length()",
           "3");

    expect(S, "trim via pattern",
           "\"   hello   \".match(\"^%s*(.-)%s*$\")",
           "hello");

    art_state_free(S);

    printf("\n%d checks, %d failed\n", checks, failed);
    return failed == 0 ? 0 : 1;
}