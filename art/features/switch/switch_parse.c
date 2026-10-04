#include "switch.h"
#include "parser.h"
#include <stdlib.h>

static Node *parse_arm_body(Parser *P)
{
    if (parser_check(P, TOKEN_LEFT_BRACE))
        return parse_block(P);
    return parse_expression(P);
}

Node *switch_parse(Parser *P)
{
    Token sw = P->previous;
    parser_consume(P, TOKEN_LEFT_PAREN, "expected '(' after 'switch'");

    Node *subject = parse_expression(P);
    if (subject == NULL)
        return NULL;

    parser_consume(P, TOKEN_RIGHT_PAREN, "expected ')' after subject");
    parser_consume(P, TOKEN_LEFT_BRACE, "expected '{' to open switch");

    int acap = 4, acount = 0;
    SwitchArm *arms = malloc(sizeof(SwitchArm) * acap);
    Node *else_body = NULL;

    while (!parser_check(P, TOKEN_RIGHT_BRACE) &&
           !parser_check(P, TOKEN_EOF))
    {
        if (parser_match(P, TOKEN_ELSE))
        {
            parser_consume(P, TOKEN_ARROW, "expected '->' after 'else'");
            else_body = parse_arm_body(P);
            parser_match(P, TOKEN_COMMA);
            continue;
        }

        int ccap = 4, ccount = 0;
        Node **cases = malloc(sizeof(Node *) * ccap);

        bool saved_switch = P->in_switch_case;
        P->in_switch_case = true;

        for (;;)
        {
            if (ccount >= ccap)
            {
                ccap *= 2;
                cases = realloc(cases, sizeof(Node *) * ccap);
            }

            Node *c = parse_expression(P);
            if (c == NULL)
            {
                P->in_switch_case = saved_switch;
                for (int i = 0; i < ccount; i++)
                    node_free_tree(&cases[i]);
                free(cases);
                return NULL;
            }
            cases[ccount++] = c;

            if (!parser_match(P, TOKEN_COMMA))
                break;
            if (parser_check(P, TOKEN_ARROW))
                break;
        }

        P->in_switch_case = saved_switch;

        parser_consume(P, TOKEN_ARROW, "expected '->' in switch arm");

        Node *body = parse_arm_body(P);
        if (body == NULL)
        {
            for (int i = 0; i < ccount; i++)
                node_free_tree(&cases[i]);
            free(cases);
            return NULL;
        }
        parser_match(P, TOKEN_COMMA);

        if (acount >= acap)
        {
            acap *= 2;
            arms = realloc(arms, sizeof(SwitchArm) * acap);
        }
        arms[acount].cases = cases;
        arms[acount].case_count = ccount;
        arms[acount].body = body;
        acount++;
    }

    parser_consume(P, TOKEN_RIGHT_BRACE, "expected '}' to close switch");

    Node *result = node_switch(sw.line, sw.column, subject,
                               arms, acount, else_body);
    free(arms);
    return result;
}
