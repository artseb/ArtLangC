// ============================================================
// parser.c — top-level entry point
//
// Drives parse_source: init state, walk statements until EOF,
// recover on error, return the AST or NULL.
//
// The statement loop has a no-progress guard so a malformed
// input can never hang. If a statement parse returns NULL and
// neither the statement parser nor parser_synchronize advanced
// the lexer, we force-advance one token to guarantee forward
// motion.
// ============================================================

#include <stdlib.h>
#include <string.h>
#include "parser.h"

Node *parse_source(ArtState *S, const char *source, int length,
                   const char *file_name)
{
    parser_init(S);

    Parser P;
    P.state = S;
    P.file_name = file_name;
    P.had_error = false;
    P.error_count = 0;
    P.panic_mode = false;
    P.in_switch_case = false;
    P.paren_depth = 0;
    P.depth = 0;

    lexer_init(&P.lexer, S, source, length);

    P.current = lexer_next(&P.lexer);
    while (P.current.type == TOKEN_ERROR)
    {
        parser_error_at(&P, &P.current, "%.*s",
                        P.current.length, P.current.start);
        P.current = lexer_next(&P.lexer);
    }

    int cap = 16, count = 0;
    Node **stmts = malloc(sizeof(Node *) * cap);

    while (!parser_check(&P, TOKEN_EOF))
    {
        int saved_pos = P.lexer.current;

        if (count >= cap)
        {
            cap *= 2;
            stmts = realloc(stmts, sizeof(Node *) * cap);
        }

        Node *stmt = parser_parse_statement(&P);
        if (stmt == NULL)
        {
            parser_synchronize(&P);
            if (parser_check(&P, TOKEN_EOF))
                break;

            // No progress through either the statement parser
            // or synchronize. Force one token forward so we
            // can't loop.
            if (P.lexer.current == saved_pos)
                parser_advance(&P);

            continue;
        }
        stmts[count++] = stmt;
    }

    Node *result = node_block(1, 1, stmts, count);
    free(stmts);

    if (P.had_error)
    {
        node_free_tree(&result);
        return NULL;
    }
    return result;
}
