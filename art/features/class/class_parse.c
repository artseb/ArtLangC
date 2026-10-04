#include "class.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>

void class_parse_modifiers(Parser *P, bool *is_local, bool *is_static,
                                  bool *is_const)
{
    *is_local = *is_static = *is_const = false;
    if (parser_match(P, TOKEN_LOCAL))
        *is_local = true;
    if (parser_match(P, TOKEN_STATIC))
        *is_static = true;
    if (parser_match(P, TOKEN_CONST))
        *is_const = true;
}

Node *class_parse(Parser *P)
{
    Token class_tok = P->previous;
    Token name_tok = parser_consume(P, TOKEN_IDENT,
                                    "expected class name after 'class'");
    ObjString *name = parser_token_to_string(P, &name_tok);

    ObjString *super_name = NULL;
    if (parser_match(P, TOKEN_EXTENDS))
    {
        Token st = parser_consume(P, TOKEN_IDENT,
                                  "expected superclass name after 'extends'");
        super_name = parser_token_to_string(P, &st);
    }

    int icap = 4, icount = 0;
    ObjString **impls = NULL;

    if (parser_match(P, TOKEN_IMPLEMENTS))
    {
        impls = malloc(sizeof(ObjString *) * icap);
        for (;;)
        {
            Token t = parser_consume(P, TOKEN_IDENT,
                                     "expected interface name");
            if (icount >= icap)
            {
                icap *= 2;
                impls = realloc(impls, sizeof(ObjString *) * icap);
            }
            impls[icount++] = parser_token_to_string(P, &t);
            if (!parser_match(P, TOKEN_COMMA))
                break;
        }
    }

    parser_consume(P, TOKEN_LEFT_BRACE, "expected '{' after class header");

    int fcap = 8, fcount = 0;
    Node **fields = malloc(sizeof(Node *) * fcap);
    int mcap = 8, mcount = 0;
    Node **methods = malloc(sizeof(Node *) * mcap);

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        bool is_local = false, is_static = false, is_const = false;
        class_parse_modifiers(P, &is_local, &is_static, &is_const);

        if (parser_check(P, TOKEN_IDENT))
        {
            Token fn = P->current;
            parser_advance(P);
            ObjString *fname = parser_token_to_string(P, &fn);

            Node *def = NULL;
            if (parser_match(P, TOKEN_EQUAL))
                def = parse_expression(P);
            parser_match(P, TOKEN_SEMICOLON);

            if (fcount >= fcap)
            {
                fcap *= 2;
                fields = realloc(fields, sizeof(Node *) * fcap);
            }
            fields[fcount++] = node_field_decl(
                fn.line, fn.column, fname, def,
                is_local, is_static, is_const);
            continue;
        }

        bool is_get = false, is_set = false, is_op = false;
        TokenType op = TOKEN_EOF;

        if (parser_match(P, TOKEN_FUN))
        {
        }
        else if (parser_match(P, TOKEN_GET))
            is_get = true;
        else if (parser_match(P, TOKEN_SET))
            is_set = true;
        else if (parser_match(P, TOKEN_OPERATOR))
        {
            is_op = true;
            op = P->current.type;
            parser_advance(P);
        }
        else
        {
            parser_error(P, "expected 'fun', 'get', 'set', or 'operator' in class body");
            parser_synchronize(P);
            continue;
        }

        ObjString *mname = NULL;
        if (!is_op)
        {
            Token mn = P->current;
            char first = (mn.length > 0 && mn.start) ? mn.start[0] : '\0';
            bool ident_like =
                (first >= 'a' && first <= 'z') ||
                (first >= 'A' && first <= 'Z') ||
                first == '_';

            if (!ident_like)
            {
                parser_error(P, "expected method name after 'fun'");
                parser_synchronize(P);
                continue;
            }

            parser_advance(P);
            mname = obj_string_from_utf8(P->state, mn.start, mn.length);
        }

        parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after method name");

        int pcap = 4, pcount = 0;
        ObjString **pnames = NULL;
        ObjString **ptypes = NULL;
        Node **pdefs = NULL;
        bool variadic = false;

        if (!parser_check(P, TOKEN_RIGHT_PAREN))
        {
            pnames = malloc(sizeof(ObjString *) * pcap);
            ptypes = malloc(sizeof(ObjString *) * pcap);
            pdefs = malloc(sizeof(Node *) * pcap);

            bool seen_default = false;

            for (;;)
            {
                if (pcount >= pcap)
                {
                    pcap *= 2;
                    pnames = realloc(pnames, sizeof(ObjString *) * pcap);
                    ptypes = realloc(ptypes, sizeof(ObjString *) * pcap);
                    pdefs = realloc(pdefs, sizeof(Node *) * pcap);
                }

                if (parser_match(P, TOKEN_DOT_DOT_DOT))
                {
                    Token rn = parser_consume(P, TOKEN_IDENT,
                                              "expected name after '...'");
                    pnames[pcount] = parser_token_to_string(P, &rn);
                    ptypes[pcount] = NULL;
                    pdefs[pcount] = NULL;
                    pcount++;
                    variadic = true;
                    break;
                }

                Token pn = parser_consume(P, TOKEN_IDENT,
                                          "expected parameter name");
                pnames[pcount] = parser_token_to_string(P, &pn);
                ptypes[pcount] = NULL;
                pdefs[pcount] = NULL;

                if (parser_match(P, TOKEN_COLON))
                {
                    Token tn = parser_consume(P, TOKEN_IDENT,
                                              "expected type name after ':'");
                    ptypes[pcount] = parser_token_to_string(P, &tn);
                }

                if (parser_match(P, TOKEN_EQUAL))
                {
                    pdefs[pcount] = parse_expression(P);
                    seen_default = true;
                }
                else if (seen_default)
                {
                    parser_error(P,
                                 "parameter without default after a default parameter");
                }

                pcount++;

                if (!parser_match(P, TOKEN_COMMA))
                    break;
                if (parser_check(P, TOKEN_RIGHT_PAREN))
                    break;
            }
        }

        parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after parameters");

        ObjFunction *fn = parse_function_body(P, mname, pnames, pcount, variadic);
        if (fn != NULL)
        {
            for (int i = 0; i < pcount; i++)
            {
                fn->params[i].type_name = ptypes[i];
                fn->params[i].default_value = pdefs[i];
                fn->params[i].type_class = NULL;
            }
            fn->is_static = is_static;
            fn->is_private = is_local;
            fn->is_getter = is_get;
            fn->is_setter = is_set;
            fn->is_operator = is_op;
            fn->operator_op = is_op ? (int)op : -1;
        }
        free(pnames);
        free(ptypes);
        free(pdefs);

        if (fn == NULL)
            continue;

        if (mcount >= mcap)
        {
            mcap *= 2;
            methods = realloc(methods, sizeof(Node *) * mcap);
        }
        methods[mcount++] = node_method_decl(P->previous.line,
                                             P->previous.column, fn);
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' after class body");

    Node *result = node_class_decl(class_tok.line, class_tok.column,
                                   name, super_name,
                                   impls, icount,
                                   fields, fcount, methods, mcount);
    free(impls);
    free(fields);
    free(methods);
    return result;
}
