// ============================================================
// test_lexer.c — standalone tests for the scanner
//
// Feeds short source strings through the lexer and checks the
// resulting token stream. No parser, no interpreter, just
// source bytes -> tokens.
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "value.h"
#include "token.h"
#include "lexer.h"

static int g_run = 0;
static int g_failed = 0;

#define CHECK(cond, msg)                          \
    do                                            \
    {                                             \
        g_run++;                                  \
        if (!(cond))                              \
        {                                         \
            g_failed++;                           \
            fprintf(stderr, "FAIL [%s:%d]: %s\n", \
                    __FILE__, __LINE__, (msg));   \
        }                                         \
    } while (0)

// Lex a source string. Returns a malloc'd array of tokens with
// a trailing EOF. Caller frees the array (not the tokens — they
// point into the source string).
static Token *lex_all(ArtState *S, const char *src, int *out_count)
{
    Lexer L;
    lexer_init(&L, S, src, (int)strlen(src));

    int cap = 32;
    int n = 0;
    Token *arr = malloc(sizeof(Token) * cap);

    for (;;)
    {
        Token t = lexer_next(&L);
        if (n + 1 > cap)
        {
            cap *= 2;
            arr = realloc(arr, sizeof(Token) * cap);
        }
        arr[n++] = t;
        if (t.type == TOKEN_EOF || t.type == TOKEN_ERROR)
            break;
    }

    *out_count = n;
    return arr;
}

// ============================================================
// Basic token recognition
// ============================================================

static void test_empty_input(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "", &n);
    CHECK(n == 1, "empty source -> one token");
    CHECK(t[0].type == TOKEN_EOF, "empty source -> EOF");
    free(t);
    art_state_free(S);
}

static void test_punctuation(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "(){}[],.;:", &n);
    CHECK(n == 11, "punctuation count (10 tokens + EOF)");
    CHECK(t[0].type == TOKEN_LEFT_PAREN, "(");
    CHECK(t[1].type == TOKEN_RIGHT_PAREN, ")");
    CHECK(t[2].type == TOKEN_LEFT_BRACE, "{");
    CHECK(t[3].type == TOKEN_RIGHT_BRACE, "}");
    CHECK(t[4].type == TOKEN_LEFT_BRACKET, "[");
    CHECK(t[5].type == TOKEN_RIGHT_BRACKET, "]");
    CHECK(t[6].type == TOKEN_COMMA, ",");
    CHECK(t[7].type == TOKEN_DOT, ".");
    CHECK(t[8].type == TOKEN_SEMICOLON, ";");
    CHECK(t[9].type == TOKEN_COLON, ":");
    free(t);
    art_state_free(S);
}

static void test_arithmetic_operators(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "+ - * / % ^", &n);
    CHECK(n == 7, "arith count");
    CHECK(t[0].type == TOKEN_PLUS, "+");
    CHECK(t[1].type == TOKEN_MINUS, "-");
    CHECK(t[2].type == TOKEN_STAR, "*");
    CHECK(t[3].type == TOKEN_SLASH, "/");
    CHECK(t[4].type == TOKEN_PERCENT, "%");
    CHECK(t[5].type == TOKEN_CARET, "^");
    CHECK(t[6].type == TOKEN_EOF, "EOF after arith");
    free(t);
    art_state_free(S);
}

static void test_comparison_operators(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "< > <= >= == !=", &n);
    CHECK(n == 7, "comparison count");
    CHECK(t[0].type == TOKEN_LESS, "<");
    CHECK(t[1].type == TOKEN_GREATER, ">");
    CHECK(t[2].type == TOKEN_LESS_EQUAL, "<=");
    CHECK(t[3].type == TOKEN_GREATER_EQUAL, ">=");
    CHECK(t[4].type == TOKEN_EQUAL_EQUAL, "==");
    CHECK(t[5].type == TOKEN_BANG_EQUAL, "!=");
    free(t);
    art_state_free(S);
}

static void test_assignment_operators(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "= += -= *= /= %= ^=", &n);
    CHECK(n == 8, "assign count");
    CHECK(t[0].type == TOKEN_EQUAL, "=");
    CHECK(t[1].type == TOKEN_PLUS_EQUAL, "+=");
    CHECK(t[2].type == TOKEN_MINUS_EQUAL, "-=");
    CHECK(t[3].type == TOKEN_STAR_EQUAL, "*=");
    CHECK(t[4].type == TOKEN_SLASH_EQUAL, "/=");
    CHECK(t[5].type == TOKEN_PERCENT_EQUAL, "%=");
    CHECK(t[6].type == TOKEN_CARET_EQUAL, "^=");
    free(t);
    art_state_free(S);
}

static void test_arrow_vs_minus(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "-> - - = - >", &n);
    CHECK(t[0].type == TOKEN_ARROW, "->");
    CHECK(t[1].type == TOKEN_MINUS, "-");
    CHECK(t[2].type == TOKEN_MINUS, "-");
    CHECK(t[3].type == TOKEN_EQUAL, "=");
    CHECK(t[4].type == TOKEN_MINUS, "-");
    CHECK(t[5].type == TOKEN_GREATER, ">");
    free(t);
    art_state_free(S);
}

static void test_bang(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "! != !", &n);
    CHECK(t[0].type == TOKEN_BANG, "!");
    CHECK(t[1].type == TOKEN_BANG_EQUAL, "!=");
    CHECK(t[2].type == TOKEN_BANG, "!");
    free(t);
    art_state_free(S);
}

// ============================================================
// Keywords and identifiers
// ============================================================

static void test_keywords(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S,
                       "fun local if else while for in return break continue "
                       "and or true false nil",
                       &n);

    CHECK(n == 16, "keyword count");
    CHECK(t[0].type == TOKEN_FUN, "fun");
    CHECK(t[1].type == TOKEN_LOCAL, "local");
    CHECK(t[2].type == TOKEN_IF, "if");
    CHECK(t[3].type == TOKEN_ELSE, "else");
    CHECK(t[4].type == TOKEN_WHILE, "while");
    CHECK(t[5].type == TOKEN_FOR, "for");
    CHECK(t[6].type == TOKEN_IN, "in");
    CHECK(t[7].type == TOKEN_RETURN, "return");
    CHECK(t[8].type == TOKEN_BREAK, "break");
    CHECK(t[9].type == TOKEN_CONTINUE, "continue");
    CHECK(t[10].type == TOKEN_AND, "and");
    CHECK(t[11].type == TOKEN_OR, "or");
    CHECK(t[12].type == TOKEN_TRUE, "true");
    CHECK(t[13].type == TOKEN_FALSE, "false");
    CHECK(t[14].type == TOKEN_NIL, "nil");
    CHECK(t[15].type == TOKEN_EOF, "EOF");
    free(t);
    art_state_free(S);
}

static void test_identifiers(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "foo _bar baz123 x_y_z", &n);
    CHECK(n == 5, "identifier count");
    CHECK(t[0].type == TOKEN_IDENT, "foo");
    CHECK(t[1].type == TOKEN_IDENT, "_bar");
    CHECK(t[2].type == TOKEN_IDENT, "baz123");
    CHECK(t[3].type == TOKEN_IDENT, "x_y_z");

    // Spell check via source slice
    CHECK(t[0].length == 3 && memcmp(t[0].start, "foo", 3) == 0, "ident text");
    CHECK(t[1].length == 4 && memcmp(t[1].start, "_bar", 4) == 0, "underscore ident");
    free(t);
    art_state_free(S);
}

static void test_keyword_prefix_is_ident(void)
{
    // "funny", "ifx", "or_else" are identifiers, not keywords.
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "funny ifx or_else trueish", &n);
    CHECK(t[0].type == TOKEN_IDENT, "funny is ident");
    CHECK(t[1].type == TOKEN_IDENT, "ifx is ident");
    CHECK(t[2].type == TOKEN_IDENT, "or_else is ident");
    CHECK(t[3].type == TOKEN_IDENT, "trueish is ident");
    free(t);
    art_state_free(S);
}

// ============================================================
// Numeric literals
// ============================================================

static void test_int_literals(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5 42 1000 0", &n);
    CHECK(t[0].type == TOKEN_INT, "5 is int");
    CHECK(AS_INT(t[0].literal) == 5, "5 value");
    CHECK(AS_INT(t[1].literal) == 42, "42 value");
    CHECK(AS_INT(t[2].literal) == 1000, "1000 value");
    CHECK(AS_INT(t[3].literal) == 0, "0 value");
    free(t);
    art_state_free(S);
}

static void test_float_literals(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5.0 3.14 1e10 1.5e-3 2.0E5", &n);
    CHECK(t[0].type == TOKEN_FLOAT, "5.0 is float");
    CHECK(AS_FLOAT(t[0].literal) == 5.0, "5.0 value");
    CHECK(AS_FLOAT(t[1].literal) == 3.14, "3.14 value");
    CHECK(AS_FLOAT(t[2].literal) == 1e10, "1e10 value");
    CHECK(AS_FLOAT(t[3].literal) == 1.5e-3, "1.5e-3 value");
    CHECK(AS_FLOAT(t[4].literal) == 2.0e5, "2.0E5 value");
    free(t);
    art_state_free(S);
}

static void test_dot_vs_float(void)
{
    // "5.foo" is int(5) DOT ident(foo), not float 5.
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5.foo", &n);
    CHECK(t[0].type == TOKEN_INT, "5.foo -> int first");
    CHECK(t[1].type == TOKEN_DOT, "5.foo -> dot second");
    CHECK(t[2].type == TOKEN_IDENT, "5.foo -> ident third");
    free(t);
    art_state_free(S);
}

// ============================================================
// String literals
// ============================================================

static void test_string_basic(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "\"hello\"", &n);
    CHECK(t[0].type == TOKEN_STRING, "string token");
    CHECK(IS_STRING(t[0].literal), "string literal is ObjString");

    ObjString *s = AS_STRING(t[0].literal);
    CHECK(obj_string_length(s) == 5, "hello: 5 chars");

    char *u8 = obj_string_to_utf8(s);
    CHECK(strcmp(u8, "hello") == 0, "string content");
    free(u8);
    free(t);
    art_state_free(S);
}

static void test_string_escapes(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    // Source: "a\nb"  (6 chars: quote a backslash n b quote)
    Token *t = lex_all(S, "\"a\\nb\"", &n);
    CHECK(t[0].type == TOKEN_STRING, "escaped string token");

    ObjString *s = AS_STRING(t[0].literal);
    CHECK(obj_string_length(s) == 3, "a\\nb -> 3 chars");

    char *u8 = obj_string_to_utf8(s);
    CHECK(u8[0] == 'a' && u8[1] == '\n' && u8[2] == 'b',
          "escape \\n decoded to newline");
    free(u8);
    free(t);
    art_state_free(S);
}

static void test_string_interning(void)
{
    // Same string contents in two places must produce the same pointer.
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "\"dup\" \"dup\"", &n);
    CHECK(t[0].type == TOKEN_STRING, "first string");
    CHECK(t[1].type == TOKEN_STRING, "second string");
    CHECK(AS_STRING(t[0].literal) == AS_STRING(t[1].literal),
          "identical strings intern to same pointer");
    free(t);
    art_state_free(S);
}

static void test_string_unterminated(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "\"oops", &n);
    CHECK(t[0].type == TOKEN_ERROR, "unterminated string -> ERROR");
    free(t);
    art_state_free(S);
}

static void test_string_with_newline_is_error(void)
{
    // Raw newline inside a normal string is not allowed.
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "\"line one\nline two\"", &n);
    CHECK(t[0].type == TOKEN_ERROR, "raw newline in string -> ERROR");
    free(t);
    art_state_free(S);
}

static void test_string_utf8(void)
{
    // "café"  -- c a f é, é is 2 UTF-8 bytes, 1 codepoint.
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "\"caf\xc3\xa9\"", &n);
    CHECK(t[0].type == TOKEN_STRING, "utf8 string token");

    ObjString *s = AS_STRING(t[0].literal);
    CHECK(obj_string_length(s) == 4, "café: 4 codepoints");

    free(t);
    art_state_free(S);
}

// ============================================================
// Comments
// ============================================================

static void test_comment_line(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5 // this is ignored\n10", &n);
    CHECK(n == 3, "line comment: 3 tokens");
    CHECK(t[0].type == TOKEN_INT && AS_INT(t[0].literal) == 5, "5");
    CHECK(t[1].type == TOKEN_INT && AS_INT(t[1].literal) == 10, "10");
    CHECK(t[1].line == 2, "10 is on line 2");
    free(t);
    art_state_free(S);
}

static void test_comment_block(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5 /* ignored */ 10", &n);
    CHECK(n == 3, "block comment: 3 tokens");
    CHECK(t[0].type == TOKEN_INT, "5 after nothing");
    CHECK(t[1].type == TOKEN_INT, "10 after comment");
    free(t);
    art_state_free(S);
}

static void test_comment_block_nested(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    // Nested block comments
    Token *t = lex_all(S, "5 /* outer /* inner */ still outer */ 10", &n);
    CHECK(n == 3, "nested block comment: 3 tokens");
    CHECK(t[0].type == TOKEN_INT, "5");
    CHECK(t[1].type == TOKEN_INT, "10");
    free(t);
    art_state_free(S);
}

// ============================================================
// Line tracking
// ============================================================

static void test_line_numbers(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "a\nb\n\nc", &n);
    CHECK(t[0].line == 1, "a on line 1");
    CHECK(t[1].line == 2, "b on line 2");
    CHECK(t[2].line == 4, "c on line 4");
    free(t);
    art_state_free(S);
}

// ============================================================
// Error cases
// ============================================================

static void test_unexpected_character(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "5 @ 10", &n);
    // We stop at the first ERROR by design of lex_all().
    CHECK(t[0].type == TOKEN_INT, "5 lexes fine");
    CHECK(t[1].type == TOKEN_ERROR, "@ is error");
    free(t);
    art_state_free(S);
}

// ============================================================
// A full snippet from real ART code
// ============================================================

static void test_realistic_snippet(void)
{
    ArtState *S = art_state_new();
    const char *src =
        "fun square(n) {\n"
        "    return n * n\n"
        "}\n";
    int n = 0;
    Token *t = lex_all(S, src, &n);

    CHECK(t[0].type == TOKEN_FUN, "fun");
    CHECK(t[1].type == TOKEN_IDENT, "square");
    CHECK(t[2].type == TOKEN_LEFT_PAREN, "(");
    CHECK(t[3].type == TOKEN_IDENT, "n");
    CHECK(t[4].type == TOKEN_RIGHT_PAREN, ")");
    CHECK(t[5].type == TOKEN_LEFT_BRACE, "{");
    CHECK(t[6].type == TOKEN_RETURN, "return");
    CHECK(t[7].type == TOKEN_IDENT, "n");
    CHECK(t[8].type == TOKEN_STAR, "*");
    CHECK(t[9].type == TOKEN_IDENT, "n");
    CHECK(t[10].type == TOKEN_RIGHT_BRACE, "}");
    CHECK(t[11].type == TOKEN_EOF, "EOF");

    free(t);
    art_state_free(S);
}

static void test_arrow_after_minus(void)
{
    ArtState *S = art_state_new();
    int n = 0;
    Token *t = lex_all(S, "-->", &n);
    CHECK(t[0].type == TOKEN_MINUS, "-->  first token is MINUS");
    CHECK(t[1].type == TOKEN_ARROW, "-->  second token is ARROW");
    CHECK(t[2].type == TOKEN_EOF, "-->  then EOF");
    free(t);
    art_state_free(S);
}

// ============================================================
// Entry point
// ============================================================

int main(void)
{
    printf("ART lexer tests\n");
    printf("===============\n");

    test_empty_input();
    test_punctuation();
    test_arithmetic_operators();
    test_comparison_operators();
    test_assignment_operators();
    test_arrow_vs_minus();
    test_arrow_after_minus();
    test_bang();

    test_keywords();
    test_identifiers();
    test_keyword_prefix_is_ident();

    test_int_literals();
    test_float_literals();
    test_dot_vs_float();

    test_string_basic();
    test_string_escapes();
    test_string_interning();
    test_string_unterminated();
    test_string_with_newline_is_error();
    test_string_utf8();

    test_comment_line();
    test_comment_block();
    test_comment_block_nested();

    test_line_numbers();
    test_unexpected_character();
    test_realistic_snippet();


    printf("\n%d checks, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}