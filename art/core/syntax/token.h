#ifndef ART_TOKEN_H
#define ART_TOKEN_H

#include "value.h"

// ============================================================
// Token list — one source of truth, expanded by the preprocessor
// ============================================================

#define CORE_TOKENS(X)              \
    /* --- punctuation --- */       \
    X(LEFT_PAREN, "(")              \
    X(RIGHT_PAREN, ")")             \
    X(LEFT_BRACKET, "[")            \
    X(RIGHT_BRACKET, "]")           \
    X(LEFT_BRACE, "{")              \
    X(RIGHT_BRACE, "}")             \
    X(COMMA, ",")                   \
    X(DOT, ".")                     \
    X(COLON, ":")                   \
    X(SEMICOLON, ";")               \
    X(DOT_DOT_DOT, "...")           \
                                    \
    /* --- arithmetic --- */        \
    X(PLUS, "+")                    \
    X(MINUS, "-")                   \
    X(STAR, "*")                    \
    X(SLASH, "/")                   \
    X(PERCENT, "%")                 \
    X(CARET, "^")                   \
                                    \
    /* --- comparison --- */        \
    X(EQUAL_EQUAL, "==")            \
    X(BANG_EQUAL, "!=")             \
    X(LESS, "<")                    \
    X(GREATER, ">")                 \
    X(LESS_EQUAL, "<=")             \
    X(GREATER_EQUAL, ">=")          \
                                    \
    /* --- assignment --- */        \
    X(EQUAL, "=")                   \
    X(PLUS_EQUAL, "+=")             \
    X(MINUS_EQUAL, "-=")            \
    X(STAR_EQUAL, "*=")             \
    X(SLASH_EQUAL, "/=")            \
    X(PERCENT_EQUAL, "%=")          \
    X(CARET_EQUAL, "^=")            \
                                    \
    /* --- arrows, unary --- */     \
    X(ARROW, "->")                  \
    X(BANG, "!")                    \
                                    \
    /* --- keywords: control --- */ \
    X(IF, "if")                     \
    X(ELSE, "else")                 \
    X(WHILE, "while")               \
    X(FOR, "for")                   \
    X(IN, "in")                     \
    X(RETURN, "return")             \
    X(BREAK, "break")               \
    X(CONTINUE, "continue")         \
                                    \
    /* --- keywords: values --- */  \
    X(AND, "and")                   \
    X(OR, "or")                     \
    X(TRUE, "true")                 \
    X(FALSE, "false")               \
    X(NIL, "nil")                   \
    X(FUN, "fun")                   \
    X(LOCAL, "local")               \
    X(IS, "is")                     \
                                    \
    /* --- literals --- */          \
    X(IDENT, "identifier")          \
    X(INT, "int")                   \
    X(FLOAT, "float")               \
    X(STRING, "string")             \
                                    \
    /* --- special --- */           \
    X(ERROR, "error")               \
    X(EOF, "end of file")

// --- feature keywords (kept here so the full token set is in one place) ---

#define CONST_TOKENS(X) \
    X(CONST, "const")

#define CLASS_TOKENS(X)         \
    X(CLASS, "class")           \
    X(EXTENDS, "extends")       \
    X(IMPLEMENTS, "implements") \
    X(STATIC, "static")         \
    X(GET, "get")               \
    X(SET, "set")               \
    X(OPERATOR, "operator")     \
    X(THIS, "this")             \
    X(SUPER, "super")

#define ENUM_TOKENS(X) \
    X(ENUM, "enum")

#define SWITCH_TOKENS(X) \
    X(SWITCH, "switch")

#define INTERFACE_TOKENS(X) \
    X(INTERFACE, "interface")

#define ALL_TOKENS(X) \
    CORE_TOKENS(X)    \
    ENUM_TOKENS(X)    \
    CONST_TOKENS(X)   \
    CLASS_TOKENS(X)   \
    SWITCH_TOKENS(X)  \
    INTERFACE_TOKENS(X)

typedef enum
{
#define X(name, text) TOKEN_##name,
    ALL_TOKENS(X)
#undef X
    TOKEN_TYPE_COUNT
} TokenType;

typedef struct Token
{
    TokenType type;
    const char *start;
    int length;
    int line;
    int column;
    Value literal;
} Token;

const char *token_type_name(TokenType t);

#endif // ART_TOKEN_H
