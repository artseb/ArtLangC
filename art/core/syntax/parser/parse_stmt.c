// ============================================================
// parse_stmt.c — recursive descent for statements
//
// Statement dispatch is a table keyed on token type. Tokens with
// no entry fall through to expression-statement parsing.
//
// Multi-assignment (`a, b, c = expr`) is detected here, before
// the fallback, because it starts with an identifier and would
// otherwise parse as the first of three separate statements.
// ============================================================

#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "features/features.h"

static Node *parse_decl(Parser *P);
static Node *parse_while(Parser *P);
static Node *parse_for(Parser *P);
static Node *parse_return(Parser *P);
static Node *parse_break(Parser *P);
static Node *parse_continue(Parser *P);
static Node *parse_const_decl(Parser *P);

static ParseStmtFn stmt_table[TOKEN_TYPE_COUNT];

void parser_register_stmt(TokenType token, ParseStmtFn fn)
{
    stmt_table[token] = fn;
}

void parser_init(ArtState *S)
{
    (void)S;
    for (int i = 0; i < TOKEN_TYPE_COUNT; i++)
        stmt_table[i] = NULL;

    parser_register_stmt(TOKEN_LOCAL, parse_decl);
    parser_register_stmt(TOKEN_IF, parse_if_any);
    parser_register_stmt(TOKEN_WHILE, parse_while);
    parser_register_stmt(TOKEN_FOR, parse_for);
    parser_register_stmt(TOKEN_RETURN, parse_return);
    parser_register_stmt(TOKEN_BREAK, parse_break);
    parser_register_stmt(TOKEN_CONTINUE, parse_continue);
    parser_register_stmt(TOKEN_CONST, parse_const_decl);

    FEATURES_FOR_EACH(f)
    {
        if (f->register_stmts)
            f->register_stmts();
    }
}

// Peek at the token after the current one without consuming it.
// Saves and restores the lexer position; cheap because the lexer
// is just an index into the source.
static Token peek_next_token(Parser *P)
{
    Token saved_current = P->current;
    Lexer saved_lexer = P->lexer;

    Token next = lexer_next(&P->lexer);

    P->current = saved_current;
    P->lexer = saved_lexer;
    return next;
}

// A multi-assignment looks like: IDENT , IDENT [ , IDENT ]* = expr
// Detected by peeking: current is IDENT and the token after is
// COMMA. Nothing else valid starts that way at statement level,
// so we don't need a deeper lookahead.
static bool starts_multi_assign(Parser *P)
{
    if (!parser_check(P, TOKEN_IDENT))
        return false;
    Token next = peek_next_token(P);
    return next.type == TOKEN_COMMA;
}

static Node *parse_multi_assign(Parser *P)
{
    Token first = P->current;
    parser_advance(P);  // consume first IDENT

    int cap = 4, count = 0;
    ObjString **names = malloc(sizeof(ObjString *) * cap);
    names[count++] = parser_token_to_string(P, &first);

    while (parser_match(P, TOKEN_COMMA))
    {
        Token t = parser_consume(P, TOKEN_IDENT,
                                 "expected name after ','");
        if (count >= cap)
        {
            cap *= 2;
            names = realloc(names, sizeof(ObjString *) * cap);
        }
        names[count++] = parser_token_to_string(P, &t);
    }

    if (!parser_match(P, TOKEN_EQUAL))
    {
        parser_error(P, "expected '=' in multi-assignment");
        free(names);
        return NULL;
    }

    Node *value = parse_expression(P);
    if (value == NULL)
    {
        free(names);
        return NULL;
    }

    parser_match(P, TOKEN_SEMICOLON);
    Node *result = node_multi_decl(first.line, first.column,
                                   names, count, value, DECL_ASSIGN);
    free(names);
    return result;
}

Node *parser_parse_statement(Parser *P)
{
    if (parser_match(P, TOKEN_SEMICOLON))
        return node_literal(P->previous.line, P->previous.column, NIL_VAL);

    if (parser_check(P, TOKEN_LEFT_BRACE))
        return parse_block(P);

    ParseStmtFn fn = stmt_table[P->current.type];
    if (fn != NULL)
    {
        parser_advance(P);
        return fn(P);
    }

    if (starts_multi_assign(P))
        return parse_multi_assign(P);

    Node *expr = parse_expression(P);
    if (expr == NULL)
        return NULL;
    parser_match(P, TOKEN_SEMICOLON);
    return expr;
}

Node *parse_block(Parser *P)
{
    Token open = parser_consume(P, TOKEN_LEFT_BRACE, "expected '{' to open block");

    int cap = 8, count = 0;
    Node **stmts = malloc(sizeof(Node *) * cap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        if (count >= cap)
        {
            cap *= 2;
            stmts = realloc(stmts, sizeof(Node *) * cap);
        }

        Node *stmt = parser_parse_statement(P);
        if (stmt == NULL)
        {
            parser_synchronize(P);
            if (parser_check(P, TOKEN_EOF))
                break;
            continue;
        }
        stmts[count++] = stmt;
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' to close block");

    Node *result = node_block(open.line, open.column, stmts, count);
    free(stmts);
    return result;
}

static Node *parse_decl(Parser *P)
{
    Token local_tok = P->previous;
    bool is_const = const_try_match(P);

    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected name after 'local'");
    ObjString *first = parser_token_to_string(P, &name_tok);

    // Single-name form.
    if (!parser_check(P, TOKEN_COMMA))
    {
        Node *value = NULL;
        if (parser_match(P, TOKEN_EQUAL))
        {
            value = parse_expression(P);
            if (value == NULL)
                return NULL;
        }
        parser_match(P, TOKEN_SEMICOLON);

        uint32_t flags = DECL_LOCAL | (is_const ? DECL_CONST : 0);
        return node_decl(local_tok.line, local_tok.column,
                         first, value, flags);
    }

    // Multi-name destructuring: local a, b, c = expr
    int cap = 4, count = 1;
    ObjString **names = malloc(sizeof(ObjString *) * cap);
    names[0] = first;

    while (parser_match(P, TOKEN_COMMA))
    {
        Token t = parser_consume(P, TOKEN_IDENT,
                                 "expected name after ','");
        if (count >= cap)
        {
            cap *= 2;
            names = realloc(names, sizeof(ObjString *) * cap);
        }
        names[count++] = parser_token_to_string(P, &t);
    }

    if (!parser_match(P, TOKEN_EQUAL))
    {
        parser_error(P, "expected '=' after destructuring names");
        free(names);
        return NULL;
    }

    Node *value = parse_expression(P);
    if (value == NULL)
    {
        free(names);
        return NULL;
    }

    parser_match(P, TOKEN_SEMICOLON);

    uint32_t flags = DECL_LOCAL | (is_const ? DECL_CONST : 0);
    Node *result = node_multi_decl(local_tok.line, local_tok.column,
                                   names, count, value, flags);
    free(names);
    return result;
}

static Node *parse_while(Parser *P)
{
    Token while_tok = P->previous;
    parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after 'while'");
    Node *cond = parse_expression(P);
    if (cond == NULL)
        return NULL;
    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after condition");

    Node *body = parse_block(P);
    if (body == NULL)
        return NULL;

    return node_while(while_tok.line, while_tok.column, cond, body);
}

static Node *parse_for(Parser *P)
{
    Token for_tok = P->previous;
    parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after 'for'");

    bool is_local = parser_match(P, TOKEN_LOCAL);
    uint32_t flags = is_local ? DECL_LOCAL : 0;

    Token first_tok = parser_consume(P, TOKEN_IDENT,
                                     "expected identifier in for header");
    ObjString *first = parser_token_to_string(P, &first_tok);

    if (parser_match(P, TOKEN_EQUAL))
    {
        Node *start = parse_expression(P);
        if (start == NULL)
            return NULL;

        if (!parser_match(P, TOKEN_ARROW))
        {
            parser_error(P, "expected '->' in range for");
            node_free_tree(&start);
            return NULL;
        }

        Node *end = parse_expression(P);
        if (end == NULL)
        {
            node_free_tree(&start);
            return NULL;
        }

        Node *step = NULL;
        if (parser_match(P, TOKEN_SEMICOLON))
        {
            step = parse_expression(P);
            if (step == NULL)
            {
                node_free_tree(&start);
                node_free_tree(&end);
                return NULL;
            }
        }

        parser_consume(P, TOKEN_RIGHT_PAREN,
                       "expected ')' after for header");

        Node *body = parse_block(P);
        if (body == NULL)
        {
            node_free_tree(&start);
            node_free_tree(&end);
            node_free_tree(&step);
            return NULL;
        }

        Node *init = node_decl(first_tok.line, first_tok.column,
                               first, start, flags);
        return node_for_range(for_tok.line, for_tok.column,
                              init, end, step, body);
    }

    if (parser_match(P, TOKEN_IN))
    {
        Node *iterable = parse_expression(P);
        if (iterable == NULL)
            return NULL;
        parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after for header");

        Node *body = parse_block(P);
        if (body == NULL)
        {
            node_free_tree(&iterable);
            return NULL;
        }

        Node *value_decl = node_decl(first_tok.line, first_tok.column,
                                     first, NULL, flags);
        return node_for_in(for_tok.line, for_tok.column,
                           NULL, value_decl, iterable, body);
    }

    if (parser_match(P, TOKEN_COMMA))
    {
        Token second_tok = parser_consume(P, TOKEN_IDENT,
                                          "expected identifier after ','");
        ObjString *second = parser_token_to_string(P, &second_tok);

        parser_consume(P, TOKEN_IN, "expected 'in' in for loop");
        Node *iterable = parse_expression(P);
        if (iterable == NULL)
            return NULL;
        parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after for header");

        Node *body = parse_block(P);
        if (body == NULL)
        {
            node_free_tree(&iterable);
            return NULL;
        }

        Node *key_decl = node_decl(first_tok.line, first_tok.column,
                                   first, NULL, flags);
        Node *value_decl = node_decl(second_tok.line, second_tok.column,
                                     second, NULL, flags);
        return node_for_in(for_tok.line, for_tok.column,
                           key_decl, value_decl, iterable, body);
    }

    parser_error(P, "expected '=', ',', or 'in' in for header");
    return NULL;
}

static Node *parse_return(Parser *P)
{
    Token ret_tok = P->previous;
    Node *value = NULL;

    if (!parser_check(P, TOKEN_RIGHT_BRACE) &&
        !parser_check(P, TOKEN_EOF) &&
        !parser_check(P, TOKEN_SEMICOLON))
    {
        value = parse_expression(P);
        if (value == NULL)
            return NULL;
    }

    parser_match(P, TOKEN_SEMICOLON);
    return node_return(ret_tok.line, ret_tok.column, value);
}

static Node *parse_break(Parser *P)
{
    Token tok = P->previous;
    parser_match(P, TOKEN_SEMICOLON);
    return node_break(tok.line, tok.column);
}

static Node *parse_continue(Parser *P)
{
    Token tok = P->previous;
    parser_match(P, TOKEN_SEMICOLON);
    return node_continue(tok.line, tok.column);
}

static Node *parse_const_decl(Parser *P)
{
    Token const_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected name after 'const'");
    ObjString *name = parser_token_to_string(P, &name_tok);

    Node *value = NULL;
    if (parser_match(P, TOKEN_EQUAL))
    {
        value = parse_expression(P);
        if (value == NULL)
            return NULL;
    }

    parser_match(P, TOKEN_SEMICOLON);
    return node_decl(const_tok.line, const_tok.column,
                     name, value, DECL_CONST);
}
