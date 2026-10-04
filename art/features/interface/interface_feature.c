#include "interface.h"
#include "feature.h"
#include "parser.h"

static bool interface_eval_node(ArtState *S, Node *n, Value *out)
{
    if (n->type == NODE_INTERFACE_DECL)
    {
        *out = eval_interface_decl(S, n);
        return true;
    }
    return false;
}

static void interface_register_stmts(void)
{
    parser_register_stmt(TOKEN_INTERFACE, interface_parse);
}

Feature interface_feature = {
    .name = "interface",
    .register_stmts = interface_register_stmts,
    .free_children = interface_free_children,
    .node_name = interface_node_name,
    .gc_blacken = interface_gc_blacken,
    .gc_free_buffers = interface_gc_free_buffers,
    .eval_node = interface_eval_node,
};
