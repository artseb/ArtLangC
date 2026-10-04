#include "switch.h"
#include "feature.h"
#include "parser.h"

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
