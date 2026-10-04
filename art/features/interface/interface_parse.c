#include "interface.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>

Node *interface_parse(Parser *P)
{
    Token iface_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected interface name");
    ObjString *name = parser_token_to_string(P, &name_tok);

    int pcap = 4, pcount = 0;
    ObjString **parents = NULL;

    if (parser_match(P, TOKEN_EXTENDS))
    {
        parents = malloc(sizeof(ObjString *) * pcap);
        for (;;)
        {
            Token t = parser_consume(P, TOKEN_IDENT,
                                     "expected parent interface name");
            if (pcount >= pcap)
            {
                pcap *= 2;
                parents = realloc(parents, sizeof(ObjString *) * pcap);
            }
            parents[pcount++] = parser_token_to_string(P, &t);
            if (!parser_match(P, TOKEN_COMMA))
                break;
        }
    }

    parser_consume(P, TOKEN_LEFT_BRACE,
                   "expected '{' after interface header");

    int mcap = 8, mcount = 0;
    ObjString **mnames = malloc(sizeof(ObjString *) * mcap);
    int *marity = malloc(sizeof(int) * mcap);
    bool *mvariadic = malloc(sizeof(bool) * mcap);

    int gcap = 4, gcount = 0;
    ObjString **gnames = malloc(sizeof(ObjString *) * gcap);

    int scap = 4, scount = 0;
    ObjString **snames = malloc(sizeof(ObjString *) * scap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        if (parser_match(P, TOKEN_FUN))
        {
            Token mt = parser_consume(P, TOKEN_IDENT,
                                      "expected method name");
            ObjString *mn = parser_token_to_string(P, &mt);

            parser_consume(P, TOKEN_LEFT_PAREN, "expected '('");

            int arity = 0;
            bool variadic = false;

            if (!parser_check(P, TOKEN_RIGHT_PAREN))
            {
                for (;;)
                {
                    if (parser_match(P, TOKEN_DOT_DOT_DOT))
                    {
                        parser_consume(P, TOKEN_IDENT,
                                       "expected name after '...'");
                        variadic = true;
                        break;
                    }

                    parser_consume(P, TOKEN_IDENT, "expected parameter");
                    arity++;
                    if (parser_match(P, TOKEN_COLON))
                        parser_consume(P, TOKEN_IDENT,
                                       "expected type name");
                    if (parser_match(P, TOKEN_EQUAL))
                        parse_expression(P);
                    if (!parser_match(P, TOKEN_COMMA))
                        break;
                }
            }
            parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')'");

            if (mcount >= mcap)
            {
                mcap *= 2;
                mnames = realloc(mnames, sizeof(ObjString *) * mcap);
                marity = realloc(marity, sizeof(int) * mcap);
                mvariadic = realloc(mvariadic, sizeof(bool) * mcap);
            }
            mnames[mcount] = mn;
            marity[mcount] = arity;
            mvariadic[mcount] = variadic;
            mcount++;
        }
        else if (parser_match(P, TOKEN_GET))
        {
            Token gt = parser_consume(P, TOKEN_IDENT, "expected name");
            if (gcount >= gcap)
            {
                gcap *= 2;
                gnames = realloc(gnames, sizeof(ObjString *) * gcap);
            }
            gnames[gcount++] = parser_token_to_string(P, &gt);
        }
        else if (parser_match(P, TOKEN_SET))
        {
            Token st = parser_consume(P, TOKEN_IDENT, "expected name");
            parser_consume(P, TOKEN_LEFT_PAREN, "expected '('");
            parser_consume(P, TOKEN_IDENT, "expected parameter");
            parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')'");
            if (scount >= scap)
            {
                scap *= 2;
                snames = realloc(snames, sizeof(ObjString *) * scap);
            }
            snames[scount++] = parser_token_to_string(P, &st);
        }
        else
        {
            parser_error(P, "expected 'fun', 'get', 'set', or '}'");
            parser_synchronize(P);
        }
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' to close interface");

    Node *result = node_interface_decl(
        iface_tok.line, iface_tok.column,
        name,
        parents, pcount,
        mnames, marity, mvariadic, mcount,
        gnames, gcount,
        snames, scount);

    free(parents);
    free(mnames);
    free(marity);
    free(mvariadic);
    free(gnames);
    free(snames);
    return result;
}