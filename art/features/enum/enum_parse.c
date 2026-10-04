// ============================================================
// enum_parse.c — parse enum declarations
//
// An enum body contains only member names, optionally with
// `= <value>`:
//
//     enum Color { Red, Green, Blue }
//     enum Http  { OK = 200, NotFound = 404 }
//
// Anything else — `fun`, `class`, `interface`, a bare `{` —
// is a syntax error. The parser reports it once and skips the
// rest of the enum body, which keeps a bad declaration from
// spinning the loop on a token it can't consume.
// ============================================================

#include "enum.h"
#include "parser.h"

#include <stdlib.h>
#include <string.h>

Node *enum_parse(Parser *P)
{
    Token enum_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected enum name after 'enum'");
    ObjString *name = parser_token_to_string(P, &name_tok);

    parser_consume(P, TOKEN_LEFT_BRACE, "expected '{' after enum name");

    int cap = 8, count = 0;
    Node **members = malloc(sizeof(Node *) * cap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        // Enum bodies accept only member names. Anything else is a
        // syntax error. Before reporting, check for the constructs
        // a user is most likely to have meant (a method or nested
        // type) so the message names the problem.
        if (!parser_check(P, TOKEN_IDENT))
        {
            if (parser_check(P, TOKEN_FUN) ||
                parser_check(P, TOKEN_CLASS) ||
                parser_check(P, TOKEN_INTERFACE))
            {
                parser_error(P, "enums contain only member names; "
                                "'%s' is not allowed in an enum body",
                              token_type_name(P->current.type));
            }
            else
            {
                parser_error(P, "expected enum member name");
            }

            // Skip the rest of the enum. Once the body is malformed
            // we can't trust anything inside it, and bailing out
            // guarantees the loop terminates on any input.
            //
            // Brace-count so a nested block (like a method body)
            // doesn't fool us into stopping on its own `}`.
            int depth = 0;
            while (!parser_check(P, TOKEN_EOF))
            {
                if (parser_check(P, TOKEN_LEFT_BRACE))
                    depth++;
                else if (parser_check(P, TOKEN_RIGHT_BRACE))
                {
                    if (depth == 0)
                        break;
                    depth--;
                }
                parser_advance(P);
            }
            break;
        }

        Token m_tok = P->current;
        parser_advance(P);
        ObjString *m_name = parser_token_to_string(P, &m_tok);

        Node *m_value = NULL;
        if (parser_match(P, TOKEN_EQUAL))
        {
            m_value = parse_expression(P);
            if (m_value == NULL)
            {
                for (int i = 0; i < count; i++)
                    node_free_tree(&members[i]);
                free(members);
                return NULL;
            }
        }

        if (count >= cap)
        {
            cap *= 2;
            members = realloc(members, sizeof(Node *) * cap);
        }
        members[count++] = node_enum_member(m_tok.line, m_tok.column,
                                            m_name, m_value);

        parser_match(P, TOKEN_COMMA);
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' after enum body");

    Node *result = node_enum_decl(enum_tok.line, enum_tok.column,
                                  name, members, count);
    free(members);
    return result;
}
