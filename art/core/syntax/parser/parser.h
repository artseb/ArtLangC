#ifndef ART_PARSER_H
#define ART_PARSER_H

#include "ast.h"
#include "token.h"
#include "lexer.h"
#include "state.h"

#define PARSER_MAX_ERRORS 20

typedef struct Parser
{
    ArtState *state;
    Lexer lexer;
    Token current;
    Token previous;
    const char *file_name;

    bool had_error;
    int error_count;
    bool panic_mode;
    bool in_switch_case;
    int paren_depth;
} Parser;

void parser_init(ArtState *S);

Node *parse_source(ArtState *S, const char *source, int length,
                   const char *file_name);

typedef Node *(*ParseStmtFn)(Parser *P);

void parser_register_stmt(TokenType token, ParseStmtFn fn);

void parser_advance(Parser *P);
bool parser_check(Parser *P, TokenType type);
bool parser_match(Parser *P, TokenType type);
Token parser_consume(Parser *P, TokenType type, const char *message);
void parser_error(Parser *P, const char *fmt, ...);
void parser_error_at(Parser *P, Token *tok, const char *fmt, ...);
void parser_synchronize(Parser *P);

ObjString *parser_token_to_string(Parser *P, Token *tok);

// True if the token, when it opens a new line, continues the
// previous expression instead of starting a new statement.
// Defined in parse_helpers.c.
bool token_continues_expression(TokenType t);

Node *parse_expression(Parser *P);

Node *parser_parse_statement(Parser *P);

Node *parse_block(Parser *P);

void parser_set_suppress(bool on);

ObjFunction *parse_function_body(Parser *P, ObjString *name,
                                 ObjString **params, int param_count,
                                 bool variadic);

// Unified `if` parser. Called from parse_prefix when the parser
// sees TOKEN_IF in any position. Handles both the statement form
// (`if (c) { ... } else { ... }`) and the expression form
// (`if (c) -> a else b`). Defined in parse_expr.c.
Node *parse_if_any(Parser *P);

#endif // ART_PARSER_H
