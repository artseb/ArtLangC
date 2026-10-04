// ============================================================
// switch_ast.c — switch AST + free + node names
// ============================================================

#include "switch.h"
#include <stdlib.h>
#include <string.h>

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
