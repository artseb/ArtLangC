// ============================================================
// switch.c — the switch feature, in one file
//
// Sections, in order: ast, parse, eval, feature.
// ============================================================

#include "switch.h"
#include "parser.h"
#include "interp.h"
#include "feature.h"
#include <stdlib.h>
#include <string.h>

// ---------------- ast ----------------

// ============================================================
// switch_ast.c — switch AST + free + node names
// ============================================================


Node *node_switch(int line, int col, Node *subject,
                  SwitchArm *arms, int arm_count, Node *else_body)
{
    SwitchNode *n = (SwitchNode *)ast_alloc_node(
        sizeof(SwitchNode), NODE_SWITCH, line, col, true);

    n->subject = subject;
    n->arm_count = arm_count;
    n->else_body = else_body;

    if (arm_count > 0)
    {
        n->arms = malloc(sizeof(SwitchArm) * arm_count);
        memcpy(n->arms, arms, sizeof(SwitchArm) * arm_count);
    }
    else
    {
        n->arms = NULL;
    }

    return (Node *)n;
}

bool switch_free_children(Node *n)
{
    if (n->type != NODE_SWITCH) return false;

    SwitchNode *sw = (SwitchNode *)n;
    node_free_tree(&sw->subject);

    for (int i = 0; i < sw->arm_count; i++)
    {
        for (int j = 0; j < sw->arms[i].case_count; j++)
            node_free_tree(&sw->arms[i].cases[j]);
        free(sw->arms[i].cases);
        node_free_tree(&sw->arms[i].body);
    }
    free(sw->arms);

    if (sw->else_body)
        node_free_tree(&sw->else_body);

    return true;
}

const char *switch_node_name(NodeType t)
{
    if (t == NODE_SWITCH) return "switch";
    return NULL;
}

// ---------------- parse ----------------

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

// ---------------- eval ----------------

static bool same_value_type(Value a, Value b)
{
    if (a.type != b.type) return false;
    if (a.type != VAL_OBJ) return true;
    return AS_OBJ(a)->type == AS_OBJ(b)->type;
}

Value eval_switch(ArtState *S, Node *n)
{
    SwitchNode *sw = (SwitchNode *)n;

    Value subject = art_eval(S, sw->subject);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    int total = 0;
    for (int i = 0; i < sw->arm_count; i++)
        total += sw->arms[i].case_count;

    Value *vals = NULL;
    if (total > 0)
        vals = malloc(sizeof(Value) * total);

    int k = 0;
    for (int i = 0; i < sw->arm_count; i++)
    {
        SwitchArm *arm = &sw->arms[i];
        for (int j = 0; j < arm->case_count; j++)
        {
            Value c = art_eval(S, arm->cases[j]);
            if (S->control != CONTROL_NONE) { free(vals); return NIL_VAL; }

            if (!same_value_type(c, subject))
            {
                free(vals);
                art_runtime_error(S, arm->cases[j],
                                  "case type %s doesn't match switch type %s",
                                  value_type_name(c), value_type_name(subject));
            }

            vals[k++] = c;
        }
    }

    k = 0;
    for (int i = 0; i < sw->arm_count; i++)
    {
        SwitchArm *arm = &sw->arms[i];
        for (int j = 0; j < arm->case_count; j++)
        {
            if (value_equal(vals[k++], subject))
            {
                free(vals);
                return art_eval(S, arm->body);
            }
        }
    }

    free(vals);

    if (sw->else_body)
        return art_eval(S, sw->else_body);
    return NIL_VAL;
}

// ---------------- feature ----------------

static bool switch_eval_node(ArtState *S, Node *n, Value *out)
{
    if (n->type == NODE_SWITCH)
    {
        *out = eval_switch(S, n);
        return true;
    }
    return false;
}

static void switch_register_stmts(void)
{
    // Switch is a prefix expression, not a statement.
}

Feature switch_feature = {
    .name = "switch",
    .register_stmts = switch_register_stmts,
    .free_children = switch_free_children,
    .node_name = switch_node_name,
    .eval_node = switch_eval_node,
};
