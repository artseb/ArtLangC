// ============================================================
// parse_helpers.c — token consumption, error reporting, recovery
// ============================================================

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "parser.h"

static bool g_suppress_errors = false;

void parser_set_suppress(bool on)
{
    g_suppress_errors = on;
}

// Depth guard for recursive descent. Called at the top of
// parse_precedence (the main recursion site) and any other
// function that can recurse on user input. The limit is high
// enough that legitimate nesting works and low enough that a
// pathological input can't blow the C stack before we catch it.
bool parser_enter(Parser *P)
{
    if (P->depth >= PARSER_MAX_DEPTH)
    {
        parser_error(P, "expression nesting too deep (max %d)",
                     PARSER_MAX_DEPTH);
        return false;
    }
    P->depth++;
    return true;
}

void parser_leave(Parser *P)
{
    if (P->depth > 0)
        P->depth--;
}

void parser_advance(Parser *P)
{
    P->previous = P->current;

    if (P->previous.type == TOKEN_LEFT_PAREN ||
        P->previous.type == TOKEN_LEFT_BRACKET)
        P->paren_depth++;
    else if (P->previous.type == TOKEN_RIGHT_PAREN ||
             P->previous.type == TOKEN_RIGHT_BRACKET)
        P->paren_depth--;

    for (;;)
    {
        P->current = lexer_next(&P->lexer);
        if (P->current.type != TOKEN_ERROR)
            break;
        parser_error_at(P, &P->current, "%.*s",
                        P->current.length, P->current.start);
    }
}

bool parser_check(Parser *P, TokenType type)
{
    return P->current.type == type;
}

bool parser_match(Parser *P, TokenType type)
{
    if (!parser_check(P, type))
        return false;
    parser_advance(P);
    return true;
}

Token parser_consume(Parser *P, TokenType type, const char *message)
{
    if (P->current.type == type)
    {
        parser_advance(P);
        return P->previous;
    }
    parser_error(P, "%s", message);
    return P->current;
}

bool token_continues_expression(TokenType t)
{
    switch (t)
    {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
    case TOKEN_CARET:
    case TOKEN_EQUAL_EQUAL:
    case TOKEN_BANG_EQUAL:
    case TOKEN_LESS:
    case TOKEN_GREATER:
    case TOKEN_LESS_EQUAL:
    case TOKEN_GREATER_EQUAL:
    case TOKEN_AND:
    case TOKEN_OR:
    case TOKEN_IN:
    case TOKEN_IS:
    case TOKEN_DOT:
        return true;
    default:
        return false;
    }
}

static void report_error(Parser *P, Token *tok, const char *fmt, va_list args)
{
    if (P->panic_mode)
        return;

    P->panic_mode = true;
    P->had_error = true;
    P->error_count++;

    if (g_suppress_errors)
        return;

    if (P->error_count > PARSER_MAX_ERRORS)
        return;

    if (P->error_count == PARSER_MAX_ERRORS)
    {
        fprintf(stderr, "%s:%d:%d: error: too many errors, stopping\n",
                P->file_name ? P->file_name : "<source>",
                tok->line, tok->column);
        return;
    }

    fprintf(stderr, "%s:%d:%d: error: ",
            P->file_name ? P->file_name : "<source>",
            tok->line, tok->column);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void parser_error_at(Parser *P, Token *tok, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    report_error(P, tok, fmt, args);
    va_end(args);
}

void parser_error(Parser *P, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    report_error(P, &P->current, fmt, args);
    va_end(args);
}

// Skip tokens until we're probably at a statement boundary.
// The `previous` check catches the case where we just consumed
// a semicolon; the switch catches natural statement starts.
//
// The loop always terminates: it breaks on EOF, on a stop token
// at the current position, or by advancing. If nothing else
// fires, it advances past a token on each iteration.
void parser_synchronize(Parser *P)
{
    P->panic_mode = false;
    P->paren_depth = 0;
    P->depth = 0;

    while (P->current.type != TOKEN_EOF)
    {
        if (P->previous.type == TOKEN_SEMICOLON)
            return;

        switch (P->current.type)
        {
        case TOKEN_LOCAL:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_FOR:
        case TOKEN_RETURN:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
        case TOKEN_FUN:
        case TOKEN_LEFT_BRACE:
            return;
        default:
            break;
        }

        parser_advance(P);
    }
}

ObjString *parser_token_to_string(Parser *P, Token *tok)
{
    return obj_string_from_utf8(P->state, tok->start, tok->length);
}
