#ifndef ART_LEXER_H
#define ART_LEXER_H

#include "token.h"
#include "state.h"

// ============================================================
// Lexer
//
// Reads UTF-8 source bytes. Produces UTF-16 strings internally
// (via ObjString interning) for STRING tokens.
// ============================================================

typedef struct Lexer
{
    ArtState *state;
    const char *source;
    int length;
    int start;
    int current;
    int line;
    int line_start;
} Lexer;

void lexer_init(Lexer *L, ArtState *S, const char *source, int length);
Token lexer_next(Lexer *L);

void lexer_error(Lexer *L, Token *tok, const char *fmt, ...);

#endif // ART_LEXER_H
