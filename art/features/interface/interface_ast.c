#include "interface.h"
#include <stdlib.h>
#include <string.h>

static ObjString **copy_names(ObjString **src, int n)
{
    if (n == 0)
        return NULL;
    ObjString **dst = malloc(sizeof(ObjString *) * n);
    memcpy(dst, src, sizeof(ObjString *) * n);
    return dst;
}

Node *node_interface_decl(int line, int col,
                          ObjString *name,
                          ObjString **parents, int parent_count,
                          ObjString **method_names, int *method_arities,
                          bool *method_variadics,
                          int method_count,
                          ObjString **getter_names, int getter_count,
                          ObjString **setter_names, int setter_count)
{
    InterfaceDeclNode *n = (InterfaceDeclNode *)ast_alloc_node(
        sizeof(InterfaceDeclNode), NODE_INTERFACE_DECL, line, col, false);

    n->name = name;
    n->parents = copy_names(parents, parent_count);
    n->parent_count = parent_count;

    n->method_names = copy_names(method_names, method_count);
    n->method_arities = NULL;
    n->method_variadics = NULL;
    if (method_count > 0)
    {
        n->method_arities = malloc(sizeof(int) * method_count);
        memcpy(n->method_arities, method_arities,
               sizeof(int) * method_count);
        n->method_variadics = malloc(sizeof(bool) * method_count);
        memcpy(n->method_variadics, method_variadics,
               sizeof(bool) * method_count);
    }
    n->method_count = method_count;

    n->getter_names = copy_names(getter_names, getter_count);
    n->getter_count = getter_count;

    n->setter_names = copy_names(setter_names, setter_count);
    n->setter_count = setter_count;

    return (Node *)n;
}

bool interface_free_children(Node *n)
{
    if (n->type != NODE_INTERFACE_DECL)
        return false;

    InterfaceDeclNode *in = (InterfaceDeclNode *)n;
    free(in->parents);
    free(in->method_names);
    free(in->method_arities);
    free(in->method_variadics);
    free(in->getter_names);
    free(in->setter_names);
    return true;
}

const char *interface_node_name(NodeType t)
{
    if (t == NODE_INTERFACE_DECL)
        return "interface_decl";
    return NULL;
}