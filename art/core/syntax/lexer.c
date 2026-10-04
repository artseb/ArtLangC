// ============================================================
// lexer.c — source bytes -> tokens
//
// One pass, no backtracking. Whitespace and comments are skipped
// between tokens.
//
// Strings: the scanner is interpolation-aware. Inside a "${...}"
// sequence in a string literal, nested double-quoted strings are
// skipped as a unit so their commas, braces, and quotes don't
// terminate the outer string. The lexer produces the string
// token; the parser re-scans its source to build the interp
// AST, so the exact token boundary matters.
// ============================================================

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "lexer.h"
#include "value.h"
#include <stdlib.h>

void lexer_init(Lexer *L, ArtState *S, const char *source, int length)
{
    L->state = S;
    L->source = source;
    L->length = length;
    L->start = 0;
    L->current = 0;
    L->line = 1;
    L->line_start = 0;
}

static bool is_at_end(Lexer *L) { return L->current >= L->length; }

static char peek(Lexer *L)
{
    if (is_at_end(L))
        return '\0';
    return L->source[L->current];
}

static char peek_next(Lexer *L)
{
    if (L->current + 1 >= L->length)
        return '\0';
    return L->source[L->current + 1];
}

static char advance(Lexer *L)
{
    if (is_at_end(L))
        return '\0';
    return L->source[L->current++];
}

static bool match(Lexer *L, char expected)
{
    if (is_at_end(L))
        return false;
    if (L->source[L->current] != expected)
        return false;
    L->current++;
    return true;
}

static Token make_token(Lexer *L, TokenType type)
{
    Token t;
    t.type = type;
    t.start = L->source + L->start;
    t.length = L->current - L->start;
    t.line = L->line;
    t.column = (L->start - L->line_start) + 1;
    t.literal = NIL_VAL;
    return t;
}

static Token error_token(Lexer *L, const char *msg)
{
    Token t;
    t.type = TOKEN_ERROR;
    t.start = msg;
    t.length = (int)strlen(msg);
    t.line = L->line;
    t.column = (L->start - L->line_start) + 1;
    t.literal = NIL_VAL;
    return t;
}

static void skip_whitespace(Lexer *L)
{
    for (;;)
    {
        char c = peek(L);
        switch (c)
        {
        case ' ':
        case '\t':
        case '\r':
            advance(L);
            break;
        case '\n':
            advance(L);
            L->line++;
            L->line_start = L->current;
            break;
        case '/':
            if (peek_next(L) == '/')
            {
                while (peek(L) != '\n' && !is_at_end(L))
                    advance(L);
            }
            else if (peek_next(L) == '*')
            {
                advance(L);
                advance(L);
                int depth = 1;
                while (depth > 0 && !is_at_end(L))
                {
                    if (peek(L) == '/' && peek_next(L) == '*')
                    {
                        advance(L);
                        advance(L);
                        depth++;
                    }
                    else if (peek(L) == '*' && peek_next(L) == '/')
                    {
                        advance(L);
                        advance(L);
                        depth--;
                    }
                    else
                    {
                        if (peek(L) == '\n')
                        {
                            L->line++;
                            advance(L);
                            L->line_start = L->current;
                        }
                        else
                        {
                            advance(L);
                        }
                    }
                }
            }
            else
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static TokenType keyword_type(const char *start, int length)
{
    static const struct
    {
        const char *text;
        TokenType type;
    } kw[] = {
#define X(name, text) {text, TOKEN_##name},
        ALL_TOKENS(X)
#undef X
    };

    for (size_t i = 0; i < sizeof(kw) / sizeof(kw[0]); i++)
    {
        if (kw[i].type == TOKEN_ERROR || kw[i].type == TOKEN_EOF)
            continue;

        if (kw[i].text[0] >= 'a' && kw[i].text[0] <= 'z')
        {
            int len = (int)strlen(kw[i].text);
            if (len == length && memcmp(kw[i].text, start, len) == 0)
                return kw[i].type;
        }
    }
    return TOKEN_IDENT;
}

static Token scan_identifier(Lexer *L)
{
    while (isalnum((unsigned char)peek(L)) || peek(L) == '_')
    {
        advance(L);
    }
    TokenType t = keyword_type(L->source + L->start, L->current - L->start);
    return make_token(L, t);
}

static Token scan_number(Lexer *L)
{
    while (isdigit((unsigned char)peek(L)))
        advance(L);

    bool is_float = false;

    if (peek(L) == '.' && isdigit((unsigned char)peek_next(L)))
    {
        is_float = true;
        advance(L);
        while (isdigit((unsigned char)peek(L)))
            advance(L);
    }

    if (peek(L) == 'e' || peek(L) == 'E')
    {
        int save = L->current;
        advance(L);
        if (peek(L) == '+' || peek(L) == '-')
            advance(L);
        if (isdigit((unsigned char)peek(L)))
        {
            is_float = true;
            while (isdigit((unsigned char)peek(L)))
                advance(L);
        }
        else
        {
            L->current = save;
        }
    }

    Token t = make_token(L, is_float ? TOKEN_FLOAT : TOKEN_INT);

    char buf[64];
    int len = L->current - L->start;
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, L->source + L->start, len);
    buf[len] = '\0';

    if (is_float)
    {
        t.literal = FLOAT_VAL(strtod(buf, NULL));
    }
    else
    {
        t.literal = INT_VAL(strtoll(buf, NULL, 10));
    }
    return t;
}

static Token scan_triple_string(Lexer *L)
{
    int content_start = L->current;
    int content_line = L->line;

    while (!is_at_end(L))
    {
        if (peek(L) == '"' &&
            peek_next(L) == '"' &&
            L->current + 2 < L->length &&
            L->source[L->current + 2] == '"')
        {
            int content_len = L->current - content_start;

            ObjString *s = obj_string_from_utf8(L->state,
                                                L->source + content_start,
                                                content_len);

            advance(L);
            advance(L);
            advance(L);

            Token t = make_token(L, TOKEN_STRING);
            t.line = content_line;
            t.literal = OBJ_VAL(s);
            return t;
        }

        if (peek(L) == '\n')
        {
            advance(L);
            L->line++;
            L->line_start = L->current;
        }
        else
        {
            advance(L);
        }
    }

    return error_token(L, "unterminated multi-line string");
}

static Token scan_string(Lexer *L)
{
    if (peek(L) == '"' && peek_next(L) == '"')
    {
        advance(L);
        advance(L);
        return scan_triple_string(L);
    }

    int buf_cap = 32;
    int buf_len = 0;
    char *buf = malloc(buf_cap);

    while (!is_at_end(L))
    {
        char c = peek(L);

        if (c == '\n')
        {
            free(buf);
            return error_token(L, "unterminated string");
        }

        // Interpolation. When we see "${", enter a small scanner
        // that finds the matching "}" while tracking brace depth
        // and skipping nested strings. Anything inside is left in
        // the source; the parser re-scans it. This is what lets
        //   "${names.join(", ")}"
        // work without the inner quotes ending the outer string.
        if (c == '$' && peek_next(L) == '{')
        {
            advance(L); // $
            advance(L); // {
            int depth = 1;
            while (depth > 0 && !is_at_end(L))
            {
                char d = peek(L);

                if (d == '\\' && L->current + 1 < L->length)
                {
                    advance(L);
                    advance(L);
                    continue;
                }

                if (d == '"')
                {
                    advance(L);
                    while (!is_at_end(L) && peek(L) != '"')
                    {
                        if (peek(L) == '\\' && L->current + 1 < L->length)
                        {
                            advance(L);
                            advance(L);
                            continue;
                        }
                        if (peek(L) == '\n')
                        {
                            free(buf);
                            return error_token(L, "unterminated string");
                        }
                        advance(L);
                    }
                    if (!is_at_end(L))
                        advance(L); // closing "
                    continue;
                }

                if (d == '{')
                    depth++;
                else if (d == '}')
                    depth--;
                if (depth == 0)
                    break;
                advance(L);
            }

            if (depth != 0)
            {
                free(buf);
                return error_token(L, "unterminated interpolation");
            }
            advance(L); // closing }
            continue;
        }

        if (c == '"')
            break;

        if (c == '\\')
        {
            advance(L);
            char esc = peek(L);
            switch (esc)
            {
            case 'n':
                c = '\n';
                advance(L);
                break;
            case 't':
                c = '\t';
                advance(L);
                break;
            case 'r':
                c = '\r';
                advance(L);
                break;
            case '0':
                c = '\0';
                advance(L);
                break;
            case '\\':
                c = '\\';
                advance(L);
                break;
            case '"':
                c = '"';
                advance(L);
                break;
            case '\'':
                c = '\'';
                advance(L);
                break;
            case '$':
                c = '$';
                advance(L);
                break;
            default:
                free(buf);
                return error_token(L, "unknown escape sequence");
            }
            if (buf_len + 1 > buf_cap)
            {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap);
            }
            buf[buf_len++] = c;
            continue;
        }

        if (buf_len + 1 > buf_cap)
        {
            buf_cap *= 2;
            buf = realloc(buf, buf_cap);
        }
        buf[buf_len++] = c;
        advance(L);
    }

    if (is_at_end(L))
    {
        free(buf);
        return error_token(L, "unterminated string");
    }

    advance(L); // closing "

    ObjString *s = obj_string_from_utf8(L->state, buf, buf_len);
    free(buf);

    Token t = make_token(L, TOKEN_STRING);
    t.literal = OBJ_VAL(s);
    return t;
}

Token lexer_next(Lexer *L)
{
    skip_whitespace(L);

    L->start = L->current;

    if (is_at_end(L))
        return make_token(L, TOKEN_EOF);

    char c = advance(L);

    if (isalpha((unsigned char)c) || c == '_')
    {
        return scan_identifier(L);
    }

    if (isdigit((unsigned char)c))
    {
        return scan_number(L);
    }

    if (c == '"')
        return scan_string(L);

    switch (c)
    {
    case '(':
        return make_token(L, TOKEN_LEFT_PAREN);
    case ')':
        return make_token(L, TOKEN_RIGHT_PAREN);
    case '[':
        return make_token(L, TOKEN_LEFT_BRACKET);
    case ']':
        return make_token(L, TOKEN_RIGHT_BRACKET);
    case '{':
        return make_token(L, TOKEN_LEFT_BRACE);
    case '}':
        return make_token(L, TOKEN_RIGHT_BRACE);
    case ',':
        return make_token(L, TOKEN_COMMA);
    case '.':
        if (match(L, '.') && match(L, '.'))
            return make_token(L, TOKEN_DOT_DOT_DOT);
        return make_token(L, TOKEN_DOT);
    case ':':
        return make_token(L, TOKEN_COLON);
    case ';':
        return make_token(L, TOKEN_SEMICOLON);
    case '+':
        return make_token(L, match(L, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
    case '-':
        if (match(L, '>'))
            return make_token(L, TOKEN_ARROW);
        return make_token(L, match(L, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '*':
        return make_token(L, match(L, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
    case '/':
        return make_token(L, match(L, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
    case '%':
        return make_token(L, match(L, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
    case '^':
        return make_token(L, match(L, '=') ? TOKEN_CARET_EQUAL : TOKEN_CARET);
    case '!':
        return make_token(L, match(L, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
        return make_token(L, match(L, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<':
        return make_token(L, match(L, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>':
        return make_token(L, match(L, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    }

    return error_token(L, "unexpected character");
}

static const char *const TOKEN_NAMES[] = {
#define X(name, text) text,
    ALL_TOKENS(X)
#undef X
};

const char *token_type_name(TokenType t)
{
    if (t < 0 || t >= (TokenType)(sizeof(TOKEN_NAMES) / sizeof(TOKEN_NAMES[0])))
    {
        return "unknown";
    }
    return TOKEN_NAMES[t];
}

void lexer_error(Lexer *L, Token *tok, const char *fmt, ...)
{
    fprintf(stderr, "art:%d: error: ", tok->line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    (void)L;
}
