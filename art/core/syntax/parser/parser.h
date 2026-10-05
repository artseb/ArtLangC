#ifndef ART_PARSER_H
#define ART_PARSER_H

#include "ast.h"
#include "token.h"
#include "lexer.h"
#include "state.h"

#define PARSER_MAX_ERRORS 20
#define PARSER_MAX_DEPTH 256

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
    int depth; // recursion depth in parse_precedence
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

// Depth tracking for recursive descent. parser_enter returns
// false and raises an error when the limit is exceeded;
// parser_leave must be called symmetrically.
bool parser_enter(Parser *P);
void parser_leave(Parser *P);

ObjString *parser_token_to_string(Parser *P, Token *tok);

bool token_continues_expression(TokenType t);

Node *parse_expression(Parser *P);

Node *parser_parse_statement(Parser *P);

Node *parse_block(Parser *P);

void parser_set_suppress(bool on);

ObjFunction *parse_function_body(Parser *P, ObjString *name,
                                 ObjString **params, int param_count,
                                 bool variadic);

Node *parse_if_any(Parser *P);

#endif // ART_PARSER_H